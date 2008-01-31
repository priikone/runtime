/*

  silchashtable.c

  Author: Pekka Riikonen <priikone@silcnet.org>

  Copyright (C) 2001 - 2008 Pekka Riikonen

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

*/
/* Implementation of collision resistant hash table. This is a hash table
   that provides a reliable (what you add stays there, and duplicate keys
   are allowed) with as fast reference to the key as possible. If duplicate
   keys are a lot in the hash table the lookup gets slower of course.
   However, this is reliable and no data is lost at any point. If you know
   that you never have duplicate keys then this is as fast as any simple
   hash table. */

#include "silcruntime.h"

/* Define to 1 if you want hash table debug enabled */
#define SILC_HASH_TABLE_DEBUG 0

#if SILC_HASH_TABLE_DEBUG == 1
#define SILC_HT_DEBUG(fmt) SILC_LOG_DEBUG(fmt)
#else
#define SILC_HT_DEBUG(fmt)
#endif

/* Default size of the hash table (index to prime table) */
#define SILC_HASH_TABLE_SIZE 2

/* Produce the index by hashing the key */
#define SILC_HASH_TABLE_HASH(f, c) \
  ((f)(key, (c)) % primesize[ht->table_size])

/* Check whether need to rehash */
#define SILC_HASH_REHASH_INC \
  (ht->auto_rehash && (ht->entry_count / 2) > primesize[ht->table_size])
#define SILC_HASH_REHASH_DEC \
  (ht->auto_rehash && (ht->entry_count * 2) < primesize[ht->table_size] && \
   ht->entry_count > primesize[SILC_HASH_TABLE_SIZE])

/* One entry in the hash table. Includes the key and the associated
   context. The `next' pointer is non-NULL if two (or more) different
   keys hashed to same value.  The pointer is the pointer to the next
   entry. */
typedef struct SilcHashTableEntryStruct {
  void *key;
  void *context;
  struct SilcHashTableEntryStruct *next;
} *SilcHashTableEntry;

/* Hash table. */
struct SilcHashTableStruct {
  SilcStack stack;
  SilcHashTableEntry *table;
  SilcUInt32 table_size;
  SilcUInt32 entry_count;
  SilcHashFunction hash;
  SilcHashCompare compare;
  SilcHashDestructor destructor;
  void *hash_user_context;
  void *compare_user_context;
  void *destructor_user_context;
  unsigned int auto_rehash : 1;
};

/* Prime sizes for the hash table. The size of the table will always
   be one of these. */
const SilcUInt32 primesize[] =
{
  3, 5, 11, 17, 37, 67, 109, 131, 163, 257, 367, 521, 823, 1031,
  1237, 1447, 2053, 2389, 2777, 3323, 4099, 5059, 6247, 7001, 8209, 10993,
  14057, 16411, 19181, 21089, 25033, 32771, 40009, 47431, 65537, 106721,
  131101, 262147, 360163, 524309, 810343, 1048583, 2097169, 4194319,
  6153409, 8388617, 13845163, 16777259, 33554467, 67108879
};

/* Find appropriate size for the hash table. The size will be a prime. */

static SilcUInt32 silc_hash_table_primesize(SilcUInt32 size, SilcUInt32 *index)
{
  int i;

  for (i = 0; i < sizeof(primesize) / sizeof(primesize[0]); i++)
    if (primesize[i] >= size) {
      *index = i;
      SILC_HT_DEBUG(("sizeof of the hash table is %d", primesize[*index]));
      return primesize[i];
    }

  *index = i - 1;
  SILC_HT_DEBUG(("sizeof of the hash table is %d", primesize[*index]));
  return primesize[i - 1];
}

/* Internal routine to find entry in the hash table by `key'. Returns
   the previous entry (if exists) as well. */

static inline SilcHashTableEntry *
silc_hash_table_find_internal(SilcHashTable ht, void *key,
			      SilcHashTableEntry *prev_entry,
			      SilcHashFunction hash, void *hash_user_context,
			      SilcHashCompare compare,
			      void *compare_user_context)
{
  SilcHashTableEntry *entry, prev = NULL;
  SilcUInt32 i = SILC_HASH_TABLE_HASH(hash, hash_user_context);

  SILC_HT_DEBUG(("index %d key %p", i, key));

  entry = &ht->table[i];
  if (compare) {
    while (*entry && !compare((*entry)->key, key, compare_user_context)) {
      prev = *entry;
      entry = &(*entry)->next;
    }
  } else {
    while (*entry && (*entry)->key != key) {
      prev = *entry;
      entry = &(*entry)->next;
    }
  }

  *prev_entry = prev;
  return entry;
}

/* Internal routine to find entry in the hash table by `key' and `context'.
   Returns the previous entry (if exists) as well to `prev_entry'. */

static inline SilcHashTableEntry *
silc_hash_table_find_internal_context(SilcHashTable ht, void *key,
				      void *context,
				      SilcHashTableEntry *prev_entry,
				      SilcHashFunction hash,
				      void *hash_user_context,
				      SilcHashCompare compare,
				      void *compare_user_context)
{
  SilcHashTableEntry *entry, prev = NULL;
  SilcUInt32 i = SILC_HASH_TABLE_HASH(hash, hash_user_context);

  SILC_HT_DEBUG(("index %d key %p context %p", i, key, context));

  entry = &ht->table[i];
  if (ht->compare) {
    while (*entry) {
      if (compare((*entry)->key, key, compare_user_context) &&
	  (*entry)->context == context)
	break;
      prev = *entry;
      entry = &(*entry)->next;
    }
  } else {
    while (*entry) {
      if ((*entry)->key == key && (*entry)->context == context)
	break;
      prev = *entry;
      entry = &(*entry)->next;
    }
  }

  if (prev_entry)
    *prev_entry = prev;
  return entry;
}

/* Internal routine to find entry in the hash table by `key'. */

static inline SilcHashTableEntry *
silc_hash_table_find_internal_simple(SilcHashTable ht, void *key,
				     SilcHashFunction hash,
				     void *hash_user_context,
				     SilcHashCompare compare,
				     void *compare_user_context)
{
  SilcHashTableEntry *entry;
  SilcUInt32 i = SILC_HASH_TABLE_HASH(hash, hash_user_context);

  SILC_HT_DEBUG(("index %d key %p", i, key));

  entry = &ht->table[i];
  if (compare) {
    while (*entry && !compare((*entry)->key, key, compare_user_context))
      entry = &(*entry)->next;
  } else {
    while (*entry && (*entry)->key != key)
      entry = &(*entry)->next;
  }

  return entry;
}

/* Internal routine to find all keys by `key'. This may return multiple
   entries if multiple entries with same key exists. With specific
   hash and comparison functions. */

static inline void
silc_hash_table_find_internal_all(SilcHashTable ht, void *key,
				  SilcHashFunction hash,
				  void *hash_user_context,
				  SilcHashCompare compare,
				  void *compare_user_context,
				  SilcHashForeach foreach,
				  void *foreach_user_context)
{
  SilcHashTableEntry e, tmp;
  SilcBool auto_rehash, found = FALSE;
  SilcUInt32 i = SILC_HASH_TABLE_HASH(hash, hash_user_context);

  SILC_HT_DEBUG(("index %d key %p", i, key));

  /* Disallow auto rehashing while going through the table since we call
     the `foreach' function which could alter the table. */
  auto_rehash = ht->auto_rehash;
  ht->auto_rehash = FALSE;

  e = ht->table[i];
  if (compare) {
    while (e) {
      tmp = e->next;
      if (compare(e->key, key, compare_user_context)) {
	found = TRUE;
	foreach(e->key, e->context, foreach_user_context);
      }
      e = tmp;
    }
  } else {
    while (e) {
      tmp = e->next;
      if (e->key == key) {
	found = TRUE;
	foreach(e->key, e->context, foreach_user_context);
      }
      e = tmp;
    }
  }

  /* If nothing was found call with NULL context the callback */
  if (!found)
    foreach(key, NULL, foreach_user_context);

  ht->auto_rehash = auto_rehash;
}

/* Internal routine to add new key to the hash table */

static inline SilcBool
silc_hash_table_add_internal(SilcHashTable ht, void *key, void *context,
			     SilcHashFunction hash,
			     void *hash_user_context)
{
  SilcHashTableEntry *entry;
  SilcUInt32 i = SILC_HASH_TABLE_HASH(hash, hash_user_context);

  SILC_HT_DEBUG(("index %d key %p", i, key));

  entry = &ht->table[i];
  if (*entry) {
    /* The entry exists already. We have a collision, add it to the
       list to avoid collision. */
    SilcHashTableEntry e, tmp;

    e = *entry;
    tmp = e->next;
    while (tmp) {
      e = tmp;
      tmp = tmp->next;
    }

    SILC_HT_DEBUG(("Collision; adding new key to list"));

    e->next = silc_scalloc(ht->stack, 1, sizeof(*e->next));
    if (!e->next)
      return FALSE;
    e->next->key = key;
    e->next->context = context;
    ht->entry_count++;
  } else {
    /* New key */
    SILC_HT_DEBUG(("New key"));
    *entry = silc_scalloc(ht->stack, 1, sizeof(**entry));
    if (!(*entry))
      return FALSE;
    (*entry)->key = key;
    (*entry)->context = context;
    ht->entry_count++;
  }

  if (SILC_HASH_REHASH_INC)
    silc_hash_table_rehash(ht, 0);

  return TRUE;
}

/* Internal routine to replace old key with new one (if it exists) */

static inline SilcBool
silc_hash_table_replace_internal(SilcHashTable ht, void *key, void *context,
				 SilcHashFunction hash,
				 void *hash_user_context)
{
  SilcHashTableEntry *entry;
  SilcUInt32 i = SILC_HASH_TABLE_HASH(hash, hash_user_context);

  SILC_HT_DEBUG(("index %d key %p", i, key));

  entry = &ht->table[i];
  if (*entry) {
    /* The entry exists already. We have a collision, replace the old
       key and context. */
    if (ht->destructor)
      ht->destructor((*entry)->key, (*entry)->context,
		     ht->destructor_user_context);
  } else {
    /* New key */
    *entry = silc_scalloc(ht->stack, 1, sizeof(**entry));
    if (!(*entry))
      return FALSE;
    ht->entry_count++;
  }

  (*entry)->key = key;
  (*entry)->context = context;

  if (SILC_HASH_REHASH_INC)
    silc_hash_table_rehash(ht, 0);

  return TRUE;
}

/* Allocates new hash table and returns it.  If the `table_size' is not
   zero then the hash table size is the size provided. If zero then the
   default size will be used. Note that if the `table_size' is provided
   it should be a prime. The `hash', `compare' and `destructor' are
   the hash function, the key comparison function and key and context
   destructor function, respectively. The `hash' is mandatory, the others
   are optional. */

SilcHashTable silc_hash_table_alloc(SilcStack stack,
				    SilcUInt32 table_size,
				    SilcHashFunction hash,
				    void *hash_user_context,
				    SilcHashCompare compare,
				    void *compare_user_context,
				    SilcHashDestructor destructor,
				    void *destructor_user_context,
				    SilcBool auto_rehash)
{
  SilcHashTable ht;
  SilcUInt32 size_index = SILC_HASH_TABLE_SIZE;

  if (!hash) {
    silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
    return NULL;
  }

  if (stack)
    stack = silc_stack_alloc(0, stack);

  ht = silc_scalloc(stack, 1, sizeof(*ht));
  if (!ht) {
    silc_stack_free(stack);
    return NULL;
  }
  ht->table = silc_scalloc(stack,
			   table_size ? silc_hash_table_primesize(table_size,
								  &size_index) :
			   primesize[SILC_HASH_TABLE_SIZE],
			   sizeof(*ht->table));
  if (!ht->table) {
    silc_stack_free(stack);
    silc_sfree(stack, ht);
    return NULL;
  }
  ht->stack = stack;
  ht->table_size = size_index;
  ht->hash = hash;
  ht->compare = compare;
  ht->destructor = destructor;
  ht->hash_user_context = hash_user_context;
  ht->compare_user_context = compare_user_context;
  ht->destructor_user_context = destructor_user_context;
  ht->auto_rehash = auto_rehash;

  return ht;
}

/* Frees the hash table. The destructor function provided in the
   silc_hash_table_alloc will be called for all keys in the hash table. */

void silc_hash_table_free(SilcHashTable ht)
{
  SilcStack stack = ht->stack;
  SilcHashTableEntry e, tmp;
  int i;

  for (i = 0; i < primesize[ht->table_size]; i++) {
    e = ht->table[i];
    while (e) {
      if (ht->destructor)
	ht->destructor(e->key, e->context, ht->destructor_user_context);
      tmp = e;
      e = e->next;
      silc_sfree(stack, tmp);
    }
  }

  silc_sfree(stack, ht->table);
  silc_sfree(stack, ht);
  silc_stack_free(stack);
}

/* Returns the size of the hash table */

SilcUInt32 silc_hash_table_size(SilcHashTable ht)
{
  return primesize[ht->table_size];
}

/* Returns the number of the entires in the hash table. If there is more
   entries in the table thatn the size of the hash table calling the
   silc_hash_table_rehash is recommended. */

SilcUInt32 silc_hash_table_count(SilcHashTable ht)
{
  return ht->entry_count;
}

/* Adds new entry to the hash table. The `key' is hashed using the
   hash function and the both `key' and `context' will be saved to the
   hash table. This function quarantees that the entry is always added
   to the hash table reliably (it is collision resistant). */

SilcBool silc_hash_table_add(SilcHashTable ht, void *key, void *context)
{
  return silc_hash_table_add_internal(ht, key, context, ht->hash,
				      ht->hash_user_context);
}

/* Same as above but with specific hash function and user context. */

SilcBool silc_hash_table_add_ext(SilcHashTable ht, void *key, void *context,
				 SilcHashFunction hash,
				 void *hash_user_context)
{
  return silc_hash_table_add_internal(ht, key, context,
				      hash, hash_user_context);
}

/* Same as above but if the `key' already exists in the hash table
   the old key and the old context will be replace with the `key' and
   the `context. The destructor function will be called for the
   replaced key and context. */

SilcBool silc_hash_table_set(SilcHashTable ht, void *key, void *context)
{
  return silc_hash_table_replace_internal(ht, key, context, ht->hash,
					  ht->hash_user_context);
}

/* Same as above but with specific hash function. */

SilcBool silc_hash_table_set_ext(SilcHashTable ht, void *key,
				 void *context,
				 SilcHashFunction hash,
				 void *hash_user_context)
{
  return silc_hash_table_replace_internal(ht, key, context,
					  hash, hash_user_context);
}

/* Removes the entry from the hash table by the provided `key'. This will
   call the destructor funtion for the found entry. Return TRUE if the
   entry was removed successfully and FALSE otherwise. */

SilcBool silc_hash_table_del(SilcHashTable ht, void *key)
{
  SilcHashTableEntry *entry, prev, e;

  entry = silc_hash_table_find_internal(ht, key, &prev,
					ht->hash, ht->hash_user_context,
					ht->compare, ht->compare_user_context);
  if (*entry == NULL) {
    silc_set_errno(SILC_ERR_NOT_FOUND);
    return FALSE;
  }

  e = *entry;

  if (!prev && e->next)
    *entry = e->next;
  if (!prev && e->next == NULL)
    *entry = NULL;
  if (prev)
    prev->next = NULL;
  if (prev && e->next)
    prev->next = e->next;

  if (ht->destructor)
    ht->destructor(e->key, e->context, ht->destructor_user_context);
  silc_sfree(ht->stack, e);

  ht->entry_count--;

  if (SILC_HASH_REHASH_DEC)
    silc_hash_table_rehash(ht, 0);

  return TRUE;
}

/* Same as above but with specific hash and compare functions. */

SilcBool silc_hash_table_del_ext(SilcHashTable ht, void *key,
				 SilcHashFunction hash,
				 void *hash_user_context,
				 SilcHashCompare compare,
				 void *compare_user_context,
				 SilcHashDestructor destructor,
				 void *destructor_user_context)
{
  SilcHashTableEntry *entry, prev, e;

  entry = silc_hash_table_find_internal(ht, key, &prev,
					hash ? hash : ht->hash,
					hash_user_context ? hash_user_context :
					ht->hash_user_context,
					compare ? compare : ht->compare,
					compare_user_context ?
					compare_user_context :
					ht->compare_user_context);
  if (*entry == NULL) {
    silc_set_errno(SILC_ERR_NOT_FOUND);
    return FALSE;
  }

  e = *entry;

  if (!prev && e->next)
    *entry = e->next;
  if (!prev && e->next == NULL)
    *entry = NULL;
  if (prev)
    prev->next = NULL;
  if (prev && e->next)
    prev->next = e->next;

  if (destructor) {
    destructor(e->key, e->context, destructor_user_context);
  } else {
    if (ht->destructor)
      ht->destructor(e->key, e->context, ht->destructor_user_context);
  }
  silc_sfree(ht->stack, e);

  ht->entry_count--;

  if (SILC_HASH_REHASH_DEC)
    silc_hash_table_rehash(ht, 0);

  return TRUE;
}

/* Same as above but verifies that the context associated with the `key'
   matches the `context'. This is handy to use with hash tables that may
   have duplicate keys. In that case the `context' may be used to check
   whether the correct entry is being deleted. */

SilcBool silc_hash_table_del_by_context(SilcHashTable ht, void *key,
					void *context)
{
  SilcHashTableEntry *entry, prev, e;

  entry = silc_hash_table_find_internal_context(ht, key, context, &prev,
						ht->hash,
						ht->hash_user_context,
						ht->compare,
						ht->compare_user_context);
  if (*entry == NULL) {
    silc_set_errno(SILC_ERR_NOT_FOUND);
    return FALSE;
  }

  e = *entry;

  if (!prev && e->next)
    *entry = e->next;
  if (!prev && e->next == NULL)
    *entry = NULL;
  if (prev)
    prev->next = NULL;
  if (prev && e->next)
    prev->next = e->next;

  if (ht->destructor)
    ht->destructor(e->key, e->context, ht->destructor_user_context);
  silc_sfree(ht->stack, e);

  ht->entry_count--;

  if (SILC_HASH_REHASH_DEC)
    silc_hash_table_rehash(ht, 0);

  return TRUE;
}

/* Same as above but with specific hash and compare functions. */

SilcBool silc_hash_table_del_by_context_ext(SilcHashTable ht, void *key,
					    void *context,
					    SilcHashFunction hash,
					    void *hash_user_context,
					    SilcHashCompare compare,
					    void *compare_user_context,
					    SilcHashDestructor destructor,
					    void *destructor_user_context)
{
  SilcHashTableEntry *entry, prev, e;

  entry = silc_hash_table_find_internal_context(ht, key, context, &prev,
						hash ? hash : ht->hash,
						hash_user_context ?
						hash_user_context :
						ht->hash_user_context,
						compare ?
						compare : ht->compare,
						compare_user_context ?
						compare_user_context :
						ht->compare_user_context);
  if (*entry == NULL) {
    silc_set_errno(SILC_ERR_NOT_FOUND);
    return FALSE;
  }

  e = *entry;

  if (!prev && e->next)
    *entry = e->next;
  if (!prev && e->next == NULL)
    *entry = NULL;
  if (prev)
    prev->next = NULL;
  if (prev && e->next)
    prev->next = e->next;

  if (destructor) {
    destructor(e->key, e->context, destructor_user_context);
  } else {
    if (ht->destructor)
      ht->destructor(e->key, e->context, ht->destructor_user_context);
  }
  silc_sfree(ht->stack, e);

  ht->entry_count--;

  if (SILC_HASH_REHASH_DEC)
    silc_hash_table_rehash(ht, 0);

  return TRUE;
}

/* Finds the entry in the hash table by the provided `key' as fast as
   possible. Return TRUE if the entry was found and FALSE otherwise.
   The found entry is returned to the `ret_key' and `ret_context',
   respectively. If the `ret_key and `ret_context' are NULL then this
   maybe used only to check whether given key exists in the table. */

SilcBool silc_hash_table_find(SilcHashTable ht, void *key,
			      void **ret_key, void **ret_context)
{
  return silc_hash_table_find_ext(ht, key, ret_key, ret_context,
				  NULL, NULL, NULL, NULL);
}

/* Same as above but with specified hash and comparison functions. */

SilcBool silc_hash_table_find_ext(SilcHashTable ht, void *key,
				  void **ret_key, void **ret_context,
				  SilcHashFunction hash,
				  void *hash_user_context,
				  SilcHashCompare compare,
				  void *compare_user_context)
{
  SilcHashTableEntry *entry;

  entry = silc_hash_table_find_internal_simple(ht, key,
					       hash ? hash : ht->hash,
					       hash_user_context ?
					       hash_user_context :
					       ht->hash_user_context,
					       compare ? compare :
					       ht->compare,
					       compare_user_context ?
					       compare_user_context :
					       ht->compare_user_context);
  if (*entry == NULL) {
    silc_set_errno(SILC_ERR_NOT_FOUND);
    return FALSE;
  }

  if (ret_key)
    *ret_key = (*entry)->key;
  if (ret_context)
    *ret_context = (*entry)->context;

  return TRUE;
}

/* Same as silc_hash_table_find but finds with specific context. */

SilcBool silc_hash_table_find_by_context(SilcHashTable ht, void *key,
					 void *context, void **ret_key)
{
  return silc_hash_table_find_by_context_ext(ht, key, context, ret_key,
					     NULL, NULL, NULL, NULL);
}

/* Same as above but with specified hash and comparison functions. */

SilcBool silc_hash_table_find_by_context_ext(SilcHashTable ht, void *key,
					     void *context, void **ret_key,
					     SilcHashFunction hash,
					     void *hash_user_context,
					     SilcHashCompare compare,
					     void *compare_user_context)
{
  SilcHashTableEntry *entry;

  entry = silc_hash_table_find_internal_context(ht, key, context, NULL,
						hash ? hash : ht->hash,
						hash_user_context ?
						hash_user_context :
						ht->hash_user_context,
						compare ? compare :
						ht->compare,
						compare_user_context ?
						compare_user_context :
						ht->compare_user_context);
  if (!entry || !(*entry)) {
    silc_set_errno(SILC_ERR_NOT_FOUND);
    return FALSE;
  }

  if (ret_key)
    *ret_key = (*entry)->key;

  return TRUE;
}

/* As the hash table is collision resistant it is possible to save duplicate
   keys to the hash table. This function can be used to find all keys
   and contexts from the hash table that are found using the `key'. The
   `foreach' is called for every found key. */

void silc_hash_table_find_foreach(SilcHashTable ht, void *key,
				  SilcHashForeach foreach, void *user_context)
{
  silc_hash_table_find_internal_all(ht, key, ht->hash, ht->hash_user_context,
				    ht->compare, ht->compare_user_context,
				    foreach, user_context);
}

/* Same as above but with specific hash and comparison functions. */

void silc_hash_table_find_foreach_ext(SilcHashTable ht, void *key,
				      SilcHashFunction hash,
				      void *hash_user_context,
				      SilcHashCompare compare,
				      void *compare_user_context,
				      SilcHashForeach foreach,
				      void *foreach_user_context)
{
  silc_hash_table_find_internal_all(ht, key,
				    hash ? hash : ht->hash,
				    hash_user_context ?
				    hash_user_context :
				    ht->hash_user_context,
				    compare ? compare :
				    ht->compare,
				    compare_user_context ?
				    compare_user_context :
				    ht->compare_user_context,
				    foreach, foreach_user_context);
}

/* Traverse all entrys in the hash table and call the `foreach' for
   every entry with the `user_context' context. */

void silc_hash_table_foreach(SilcHashTable ht, SilcHashForeach foreach,
			     void *user_context)
{
  SilcHashTableEntry e, tmp;
  int i;
  SilcBool auto_rehash;

  if (!foreach)
    return;

  auto_rehash = ht->auto_rehash;
  ht->auto_rehash = FALSE;
  for (i = 0; i < primesize[ht->table_size]; i++) {
    e = ht->table[i];
    while (e) {
      /* Entry may become invalid inside the `foreach' */
      tmp = e->next;
      foreach(e->key, e->context, user_context);
      e = tmp;
    }
  }
  ht->auto_rehash = auto_rehash;
}

/* Rehashs the hash table. The size of the new hash table is provided
   as `new_size'. If the `new_size' is zero then this routine will make
   the new table of a suitable size. Note that this operation may be
   very slow. */

void silc_hash_table_rehash(SilcHashTable ht, SilcUInt32 new_size)
{
  int i;
  SilcHashTableEntry *table, e, tmp;
  SilcUInt32 table_size, size_index;
  SilcBool auto_rehash;

  SILC_HT_DEBUG(("Start"));

  if (new_size)
    silc_hash_table_primesize(new_size, &size_index);
  else
    silc_hash_table_primesize(ht->entry_count, &size_index);

  if (size_index == ht->table_size)
    return;

  SILC_HT_DEBUG(("Rehashing"));

  /* Take old hash table */
  table = ht->table;
  table_size = ht->table_size;
  auto_rehash = ht->auto_rehash;
  ht->auto_rehash = FALSE;

  /* Allocate new table */
  ht->table = silc_scalloc(ht->stack,
			   primesize[size_index], sizeof(*ht->table));
  if (!ht->table)
    return;
  ht->table_size = size_index;
  ht->entry_count = 0;

  /* Rehash */
  for (i = 0; i < primesize[table_size]; i++) {
    e = table[i];
    while (e) {
      silc_hash_table_add(ht, e->key, e->context);
      tmp = e;
      e = e->next;

      /* Remove old entry */
      silc_sfree(ht->stack, tmp);
    }
  }

  ht->auto_rehash = auto_rehash;

  /* Remove old table */
  silc_sfree(ht->stack, table);
}

/* Same as above but with specific hash function. */

void silc_hash_table_rehash_ext(SilcHashTable ht, SilcUInt32 new_size,
				SilcHashFunction hash,
				void *hash_user_context)
{
  int i;
  SilcHashTableEntry *table, e, tmp;
  SilcUInt32 table_size, size_index;
  SilcBool auto_rehash;

  SILC_HT_DEBUG(("Start"));

  if (new_size)
    silc_hash_table_primesize(new_size, &size_index);
  else
    silc_hash_table_primesize(ht->entry_count, &size_index);

  if (size_index == ht->table_size)
    return;

  SILC_HT_DEBUG(("Rehashing"));

  /* Take old hash table */
  table = ht->table;
  table_size = ht->table_size;
  auto_rehash = ht->auto_rehash;
  ht->auto_rehash = FALSE;

  /* Allocate new table */
  ht->table = silc_scalloc(ht->stack,
			   primesize[size_index], sizeof(*ht->table));
  if (!ht->table)
    return;
  ht->table_size = size_index;
  ht->entry_count = 0;

  /* Rehash */
  for (i = 0; i < primesize[table_size]; i++) {
    e = table[i];
    while (e) {
      silc_hash_table_add_ext(ht, e->key, e->context, hash,
			      hash_user_context);
      tmp = e;
      e = e->next;

      /* Remove old entry */
      silc_sfree(ht->stack, tmp);
    }
  }

  ht->auto_rehash = auto_rehash;

  /* Remove old table */
  silc_sfree(ht->stack, table);
}

/* Prepares the `htl' list structure sent as argument to be used in the
   hash table traversing with the silc_hash_table_get. Usage:
   SilcHashTableList htl; silc_hash_table_list(ht, &htl); */

void silc_hash_table_list(SilcHashTable ht, SilcHashTableList *htl)
{
  htl->ht = ht;
  htl->entry = NULL;
  htl->index = 0;
  htl->auto_rehash = ht->auto_rehash;

  /* Disallow rehashing of the table while traversing the table */
  ht->auto_rehash = FALSE;
}

/* Resets the `htl' SilcHashTableList. */

void silc_hash_table_list_reset(SilcHashTableList *htl)
{
  /* Set back the original auto rehash value to the table */
  htl->ht->auto_rehash = htl->auto_rehash;
}

/* Returns always the next entry in the hash table into the `key' and
   `context' and TRUE.  If this returns FALSE then there are no anymore
   any entrys. Usage: while (silc_hash_table_get(&htl, &key, &context)) */

SilcBool silc_hash_table_get(SilcHashTableList *htl, void **key,
			     void **context)
{
  SilcHashTableEntry entry = (SilcHashTableEntry)htl->entry;

  if (!htl->ht->entry_count)
    return FALSE;

  while (!entry && htl->index < primesize[htl->ht->table_size]) {
    entry = htl->ht->table[htl->index];
    htl->index++;
  }

  if (!entry)
    return FALSE;

  htl->entry = entry->next;

  if (key)
    *key = entry->key;
  if (context)
    *context = entry->context;

  return TRUE;
}

/**************************** Utility functions *****************************/

/* Case sensitive hashing */

SilcUInt32 silc_hash_string(void *key, void *user_context)
{
  char *s = (char *)key;
  SilcUInt32 h = 0;

  while (*s != '\0') {
    h += *s++;
    h += (h << 10);
    h ^= (h >> 6);
  }

  h += (h << 3);
  h ^= (h >> 11);
  h += (h << 15);

  return h;
}

/* Case-insensitive hashing */

SilcUInt32 silc_hash_string_case(void *key, void *user_context)
{
  char *s = (char *)key;
  SilcUInt32 h = 0;

  while (*s != '\0') {
    h += tolower((int)*s);
    h += (h << 10);
    h ^= (h >> 6);
    s++;
  }

  h += (h << 3);
  h ^= (h >> 11);
  h += (h << 15);

  return h;
}

/* Hash UTF-8 string */

SilcUInt32 silc_hash_utf8_string(void *key, void *user_context)
{
  char *s = (char *)key;
  SilcUInt32 h = 0;

  while (*s != '\0') {
    h += *s++;
    h += (h << 10);
    h ^= (h >> 6);
  }

  h += (h << 3);
  h ^= (h >> 11);
  h += (h << 15);

  return h;
}

/* Basic hash function to hash integers. */

SilcUInt32 silc_hash_uint(void *key, void *user_context)
{
  return SILC_PTR_TO_32(key);
}

/* Basic hash funtion to hash pointers. */

SilcUInt32 silc_hash_ptr(void *key, void *user_context)
{
  return SILC_PTR_TO_32(key);
}

/* Hash binary data. The `user_context' is the data length. */

SilcUInt32 silc_hash_data(void *key, void *user_context)
{
  SilcUInt32 len = SILC_PTR_TO_32(user_context), h, i;
  unsigned char *data = (char *)key;

  h = (data[0] * data[len - 1] + 1) * len;

  for (i = 0; i < len; i++) {
    h += data[i];
    h += (h << 10);
    h ^= (h >> 6);
  }

  h += (h << 3);
  h ^= (h >> 11);
  h += (h << 15);

  return h;
}

/* Compares two strings. */

SilcBool silc_hash_string_compare(void *key1, void *key2, void *user_context)
{
  return !strcmp((char *)key1, (char *)key2);
}

/* Compares two strings, ignores case. */

SilcBool silc_hash_string_case_compare(void *key1, void *key2,
				       void *user_context)
{
  return !strcasecmp((char *)key1, (char *)key2);
}

/* Compares binary data. */

SilcBool silc_hash_data_compare(void *key1, void *key2, void *user_context)
{
  SilcUInt32 len = SILC_PTR_TO_32(user_context);
  return !memcmp(key1, key2, len);
}

/* Compares UTF-8 string. */

SilcBool silc_hash_utf8_compare(void *key1, void *key2, void *user_context)
{
  int l1 = strlen((char *)key1);
  int l2 = strlen((char *)key2);
  if (l1 != l2)
    return FALSE;
  return !memcmp(key1, key2, l2);
}

/* Generic destructor */

void silc_hash_destructor(void *key, void *context, void *user_context)
{
  silc_free(key);
  silc_free(context);
}
