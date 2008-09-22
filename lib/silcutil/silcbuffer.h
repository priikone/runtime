/*

  silcbuffer.h

  Author: Pekka Riikonen <priikone@silcnet.org>

  Copyright (C) 1998 - 2008 Pekka Riikonen

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

*/

/****h* silcutil/Buffer Interface
 *
 * DESCRIPTION
 *
 * Data buffer interface that provides buffer allocation and manipulation
 * routines.  SilcBuffer is simple and easy to use, yet you can do to the
 * buffer almost anything you want with its method functions.  The buffer
 * is constructed of four different data sections that in whole creates
 * the allocated data area.  See the SilcBuffer context for more information.
 *
 * The SilcBuffer context is not thread-safe and if same context must be
 * used from multiple threads concurrency control must be employed.
 *
 ***/

#ifndef SILCBUFFER_H
#define SILCBUFFER_H

/****s* silcutil/SilcBuffer
 *
 * NAME
 *
 *    typedef struct { ... } *SilcBuffer, SilcBufferStruct;
 *
 * DESCRIPTION
 *
 *    SILC Buffer object. Following short description of the fields
 *    of the buffer.
 *
 *     unsiged char *head;
 *
 *        Head of the allocated buffer. This is the start of the allocated
 *        data area and remains as same throughout the lifetime of the buffer.
 *        However, the end of the head area or the start of the currently valid
 *        data area is variable.  Reallocating the buffer may change the
 *        pointer.
 *
 *        --------------------------------
 *        | head  | data         | tail  |
 *        --------------------------------
 *        ^       ^
 *
 *        Current head section in the buffer is sb->data - sb->head.
 *
 *     unsigned char *data;
 *
 *        Currently valid data area. This is the start of the currently valid
 *        main data area. The data area is variable in all directions.
 *
 *        --------------------------------
 *        | head  | data         | tail  |
 *        --------------------------------
 *                ^              ^
 *
 *        Current valid data area in the buffer is sb->tail - sb->data.
 *
 *      unsigned char *tail;
 *
 *        Tail of the buffer. This is the end of the currently valid data area
 *        or start of the tail area. The start of the tail area is variable.
 *
 *        --------------------------------
 *        | head  | data         | tail  |
 *        --------------------------------
 *                               ^       ^
 *
 *        Current tail section in the buffer is sb->end - sb->tail.
 *
 *     unsigned char *end;
 *
 *        End of the allocated buffer. This is the end of the allocated data
 *        area and remains as same throughout the lifetime of the buffer.
 *        Usually this field is not needed except when checking the size
 *        of the buffer.
 *
 *        --------------------------------
 *        | head  | data         | tail  |
 *        --------------------------------
 *                                       ^
 *
 *        Length of the entire buffer is (ie. truelen) sb->end - sb->head.
 *
 *    Currently valid data area is considered to be the main data area in
 *    the buffer. However, the entire buffer is of course valid data and can
 *    be used as such. Usually head section of the buffer includes different
 *    kind of headers or similar. Data section includes the main data of
 *    the buffer. Tail section can be seen as a reserve space of the data
 *    section. Tail section can be pulled towards end, and thus the data
 *    section becomes larger.
 *
 * SOURCE
 */
typedef struct SilcBufferObject {
  unsigned char *head;		/* Head of the allocated buffer area */
  unsigned char *data;		/* Start of the data area */
  unsigned char *tail;		/* Start of the tail area */
  unsigned char *end;		/* End of the buffer */
} *SilcBuffer, SilcBufferStruct;
/***/

/* Macros */

/****f* silcutil/silc_buffer_data
 *
 * NAME
 *
 *    unsigned char *silc_buffer_data(SilcBuffer sb)
 *
 * DESCRIPTION
 *
 *    Returns pointer to the data area of the buffer.
 *
 * SOURCE
 */
#define silc_buffer_data(x) (x)->data
/***/

/****f* silcutil/silc_buffer_tail
 *
 * NAME
 *
 *    unsigned char *silc_buffer_tail(SilcBuffer sb)
 *
 * DESCRIPTION
 *
 *    Returns pointer to the tail area of the buffer.
 *
 * SOURCE
 */
#define silc_buffer_tail(x) (x)->tail
/***/

/****f* silcutil/silc_buffer_datalen
 *
 * NAME
 *
 *    #define silc_buffer_datalen ...
 *
 * DESCRIPTION
 *
 *    Macro that can be used in function argument list to give the data
 *    pointer and the data length, instead of calling both silc_buffer_data
 *    and silc_buffer_len separately.
 *
 * EXAMPLE
 *
 *    // Following are the same thing
 *    silc_foo_function(foo, silc_buffer_datalen(buf));
 *    silc_foo_function(foo, silc_buffer_data(buf), silc_buffer_len(buf));
 *
 * SOURCE
 */
#define silc_buffer_datalen(x) (x) ? silc_buffer_data((x)) : NULL, \
  (x) ? silc_buffer_len((x)) : 0
/***/

/* Inline functions */

/****d* silcutil/silc_buffer_truelen
 *
 * NAME
 *
 *    SilcUInt32 silc_buffer_truelen(SilcBuffer sb)
 *
 * DESCRIPTION
 *
 *    Returns the true length of the buffer.
 *
 ***/
static inline
SilcUInt32 silc_buffer_truelen(SilcBuffer x)
{
  return (SilcUInt32)(x->end - x->head);
}

/****d* silcutil/silc_buffer_len
 *
 * NAME
 *
 *    SilcUInt32 silc_buffer_len(SilcBuffer sb)
 *
 * DESCRIPTION
 *
 *    Returns the current length of the data area of the buffer.
 *
 ***/
static inline
SilcUInt32 silc_buffer_len(SilcBuffer x)
{
  return (SilcUInt32)(x->tail - x->data);
}

/****d* silcutil/silc_buffer_headlen
 *
 * NAME
 *
 *    SilcUInt32 silc_buffer_headlen(SilcBuffer sb)
 *
 * DESCRIPTION
 *
 *    Returns the current length of the head data area of the buffer.
 *
 ***/
static inline
SilcUInt32 silc_buffer_headlen(SilcBuffer x)
{
  return (SilcUInt32)(x->data - x->head);
}

/****d* silcutil/silc_buffer_taillen
 *
 * NAME
 *
 *    SilcUInt32 silc_buffer_taillen(SilcBuffer sb)
 *
 * DESCRIPTION
 *
 *    Returns the current length of the tail data area of the buffer.
 *
 ***/
static inline
SilcUInt32 silc_buffer_taillen(SilcBuffer x)
{
  return (SilcUInt32)(x->end - x->tail);
}

/****f* silcutil/silc_buffer_alloc
 *
 * SYNOPSIS
 *
 *    static inline
 *    SilcBuffer silc_buffer_alloc(SilcUInt32 len);
 *
 * DESCRIPTION
 *
 *    Allocates new SilcBuffer and returns it.  Returns NULL if system is
 *    out of memory.
 *
 ***/

static inline
SilcBuffer silc_buffer_alloc(SilcUInt32 len)
{
  SilcBuffer sb;

  /* Allocate new SilcBuffer */
  sb = (SilcBuffer)silc_calloc(1, sizeof(*sb));
  if (silc_unlikely(!sb))
    return NULL;

  if (silc_likely(len)) {
    /* Allocate the actual data area */
    sb->head = (unsigned char *)silc_malloc(len * sizeof(*sb->head));
    if (silc_unlikely(!sb->head))
      return NULL;

    /* Set pointers to the new buffer */
    sb->data = sb->head;
    sb->tail = sb->head;
    sb->end = sb->head + len;
  }

  return sb;
}

/****f* silcutil/silc_buffer_salloc
 *
 * SYNOPSIS
 *
 *    static inline
 *    SilcBuffer silc_buffer_salloc(SilcStack stack, SilcUInt32 len);
 *
 * DESCRIPTION
 *
 *    Allocates new SilcBuffer and returns it.  Returns NULL if system is
 *    out of memory.
 *
 *    This routine use SilcStack are memory source.  If `stack' is NULL
 *    reverts back to normal allocating routine.
 *
 *    Note that this call consumes the `stack'.  The caller should push the
 *    stack before calling the function and pop it later.
 *
 ***/

static inline
SilcBuffer silc_buffer_salloc(SilcStack stack, SilcUInt32 len)
{
  SilcBuffer sb;

  if (!stack)
    return silc_buffer_alloc(len);

  /* Allocate new SilcBuffer */
  sb = (SilcBuffer)silc_scalloc(stack, 1, sizeof(*sb));
  if (silc_unlikely(!sb))
    return NULL;

  if (silc_likely(len)) {
    /* Allocate the actual data area */
    sb->head = (unsigned char *)silc_smalloc(stack, len * sizeof(*sb->head));
    if (silc_unlikely(!sb->head))
      return NULL;

    /* Set pointers to the new buffer */
    sb->data = sb->head;
    sb->tail = sb->head;
    sb->end = sb->head + len;
  }

  return sb;
}

/****f* silcutil/silc_buffer_free
 *
 * SYNOPSIS
 *
 *    static inline
 *    void silc_buffer_free(SilcBuffer sb);
 *
 * DESCRIPTION
 *
 *    Frees SilcBuffer.  Can be called safely `sb' as NULL.
 *
 * NOTES
 *
 *    Must not be called for buffers allocated with silc_buffer_salloc,
 *    silc_buffer_salloc_size, silc_buffer_scopy and silc_buffer_sclone.
 *    Call silc_buffer_sfree instead.
 *
 ***/

static inline
void silc_buffer_free(SilcBuffer sb)
{
  if (sb) {
#if defined(SILC_DEBUG)
    if (sb->head)
      memset(sb->head, 'F', silc_buffer_truelen(sb));
#endif
    silc_free(sb->head);
    silc_free(sb);
  }
}

/****f* silcutil/silc_buffer_sfree
 *
 * SYNOPSIS
 *
 *    static inline
 *    void silc_buffer_free(SilcStack stack, SilcBuffer sb);
 *
 * DESCRIPTION
 *
 *    Frees SilcBuffer.  If `stack' is NULL this calls silc_buffer_free.  Can
 *    be called safely `sb' as NULL.
 *
 ***/

static inline
void silc_buffer_sfree(SilcStack stack, SilcBuffer sb)
{
  if (stack) {
#ifdef SILC_DEBUG
    if (sb) {
      if (sb->head)
	memset(sb->head, 'F', silc_buffer_truelen(sb));
      memset(sb, 'F', sizeof(*sb));
    }
#endif /* SILC_DEBUG */
    return;
  }

  silc_buffer_free(sb);
}

/****f* silcutil/silc_buffer_steal
 *
 * SYNOPSIS
 *
 *    static inline
 *    unsigned char *silc_buffer_steal(SilcBuffer sb, SilcUInt32 *data_len);
 *
 * DESCRIPTION
 *
 *    Steals the data from the buffer `sb'.  This returns pointer to the
 *    start of the buffer and the true length of that buffer.  The `sb'
 *    cannot be used anymore after calling this function because the
 *    data buffer was stolen.  The `sb' must be freed with silc_buffer_free.
 *    The caller is responsible of freeing the stolen data buffer with
 *    silc_free.
 *
 ***/

static inline
unsigned char *silc_buffer_steal(SilcBuffer sb, SilcUInt32 *data_len)
{
  unsigned char *buf = sb->head;
  if (data_len)
    *data_len = silc_buffer_truelen(sb);
  sb->head = sb->data = sb->tail = sb->end = NULL;
  return buf;
}

/****f* silcutil/silc_buffer_purge
 *
 * SYNOPSIS
 *
 *    static inline
 *    void silc_buffer_purge(SilcBuffer sb);
 *
 * DESCRIPTION
 *
 *    Same as silc_buffer_free but free's only the contents of the buffer
 *    not the buffer itself.  The `sb' remains intact, data is freed.  Buffer
 *    is ready for re-use after calling this function.
 *
 * NOTES
 *
 *    Must not be called for buffers allocated with silc_buffer_salloc,
 *    silc_buffer_salloc_size, silc_buffer_scopy and silc_buffer_sclone.
 *    Use silc_buffer_spurge instead.
 *
 ***/

static inline
void silc_buffer_purge(SilcBuffer sb)
{
  silc_free(silc_buffer_steal(sb, NULL));
}

/****f* silcutil/silc_buffer_spurge
 *
 * SYNOPSIS
 *
 *    static inline
 *    void silc_buffer_spurge(SilcStack stack, SilcBuffer sb);
 *
 * DESCRIPTION
 *
 *    Same as silc_buffer_free but free's only the contents of the buffer
 *    not the buffer itself.  The `sb' remains intact, data is freed.  Buffer
 *    is ready for re-use after calling this function.  If `stack' is NULL
 *    this calls silc_buffer_purge.
 *
 ***/

static inline
void silc_buffer_spurge(SilcStack stack, SilcBuffer sb)
{
  if (stack) {
#ifdef SILC_DEBUG
    if (sb && sb->head)
      memset(silc_buffer_steal(sb, NULL), 'F', silc_buffer_truelen(sb));
#endif /* SILC_DEBUG */
    return;
  }

  silc_buffer_purge(sb);
}

/****f* silcutil/silc_buffer_set
 *
 * SYNOPSIS
 *
 *    static inline
 *    void silc_buffer_set(SilcBuffer sb,
 *			   unsigned char *data,
 *                         SilcUInt32 data_len);
 *
 * DESCRIPTION
 *
 *    Sets the `data' and `data_len' to the buffer pointer sent as argument.
 *    The data area is automatically set to the `data_len'. This function
 *    can be used to set the data to static buffer without needing any
 *    memory allocations. The `data' will not be copied to the buffer.
 *
 * EXAMPLE
 *
 *    SilcBufferStruct buf;
 *    silc_buffer_set(&buf, data, data_len);
 *
 ***/

static inline
void silc_buffer_set(SilcBuffer sb, unsigned char *data, SilcUInt32 data_len)
{
  sb->data = sb->head = data;
  sb->tail = sb->end = data + data_len;
}

/****f* silcutil/silc_buffer_pull
 *
 * SYNOPSIS
 *
 *    static inline
 *    unsigned char *silc_buffer_pull(SilcBuffer sb, SilcUInt32 len);
 *
 * DESCRIPTION
 *
 *    Pulls current data area towards end. The length of the currently
 *    valid data area is also decremented. Returns pointer to the data
 *    area before pulling. Returns NULL if the pull would lead to buffer
 *    overflow or would go beyond the valid data area.
 *
 * EXAMPLE
 *
 *    ---------------------------------
 *    | head  | data       | tail     |
 *    ---------------------------------
 *            ^
 *            Pulls the start of the data area.
 *
 *    ---------------------------------
 *    | head     | data    | tail     |
 *    ---------------------------------
 *            ^
 *
 *    silc_buffer_pull(sb, 20);
 *
 ***/

static inline
unsigned char *silc_buffer_pull(SilcBuffer sb, SilcUInt32 len)
{
  unsigned char *old_data = sb->data;

#ifdef SILC_DIST_INPLACE
  SILC_ASSERT(len <= silc_buffer_len(sb));
#endif /* SILC_DIST_INPLACE */
  if (silc_unlikely(len > silc_buffer_len(sb))) {
    silc_set_errno(SILC_ERR_OVERFLOW);
    return NULL;
  }

  sb->data += len;
  return old_data;
}

/****f* silcutil/silc_buffer_push
 *
 * SYNOPSIS
 *
 *    static inline
 *    unsigned char *silc_buffer_push(SilcBuffer sb, SilcUInt32 len);
 *
 * DESCRIPTION
 *
 *    Pushes current data area towards beginning. Length of the currently
 *    valid data area is also incremented. Returns a pointer to the
 *    data area before pushing. Returns NULL if the push would lead to
 *    go beyond the buffer boundaries or current data area.
 *
 * EXAMPLE
 *
 *    ---------------------------------
 *    | head     | data    | tail     |
 *    ---------------------------------
 *               ^
 *               Pushes the start of the data area.
 *
 *    ---------------------------------
 *    | head  | data       | tail     |
 *    ---------------------------------
 *               ^
 *
 *    silc_buffer_push(sb, 20);
 *
 ***/

static inline
unsigned char *silc_buffer_push(SilcBuffer sb, SilcUInt32 len)
{
  unsigned char *old_data = sb->data;

#ifdef SILC_DIST_INPLACE
  SILC_ASSERT((sb->data - len) >= sb->head);
#endif /* SILC_DIST_INPLACE */
  if (silc_unlikely((sb->data - len) < sb->head)) {
    silc_set_errno(SILC_ERR_OVERFLOW);
    return NULL;
  }

  sb->data -= len;
  return old_data;
}

/****f* silcutil/silc_buffer_pull_tail
 *
 * SYNOPSIS
 *
 *    static inline
 *    unsigned char *silc_buffer_pull_tail(SilcBuffer sb, SilcUInt32 len);
 *
 * DESCRIPTION
 *
 *    Pulls current tail section towards end. Length of the current valid
 *    data area is also incremented. Returns a pointer to the data area
 *    before pulling. Returns NULL if the pull would lead to buffer overflow.
 *
 * EXAMPLE
 *
 *    ---------------------------------
 *    | head  | data       | tail     |
 *    ---------------------------------
 *                         ^
 *                         Pulls the start of the tail section.
 *
 *    ---------------------------------
 *    | head  | data           | tail |
 *    ---------------------------------
 *                         ^
 *
 *    silc_buffer_pull_tail(sb, 23);
 *
 ***/

static inline
unsigned char *silc_buffer_pull_tail(SilcBuffer sb, SilcUInt32 len)
{
  unsigned char *old_tail = sb->tail;

#ifdef SILC_DIST_INPLACE
  SILC_ASSERT(len <= silc_buffer_taillen(sb));
#endif /* SILC_DIST_INPLACE */
  if (silc_unlikely(len > silc_buffer_taillen(sb))) {
    silc_set_errno(SILC_ERR_OVERFLOW);
    return NULL;
  }

  sb->tail += len;
  return old_tail;
}

/****f* silcutil/silc_buffer_push_tail
 *
 * SYNOPSIS
 *
 *    static inline
 *    unsigned char *silc_buffer_push_tail(SilcBuffer sb, SilcUInt32 len);
 *
 * DESCRIPTION
 *
 *    Pushes current tail section towards beginning. Length of the current
 *    valid data area is also decremented. Returns a pointer to the
 *    tail section before pushing. Returns NULL if the push would lead to
 *    go beyond buffer boundaries or current tail area.
 *
 * EXAMPLE
 *
 *    ---------------------------------
 *    | head  | data           | tail |
 *    ---------------------------------
 *                             ^
 *                             Pushes the start of the tail section.
 *
 *    ---------------------------------
 *    | head  | data       | tail     |
 *    ---------------------------------
 *                             ^
 *
 *    silc_buffer_push_tail(sb, 23);
 *
 ***/

static inline
unsigned char *silc_buffer_push_tail(SilcBuffer sb, SilcUInt32 len)
{
  unsigned char *old_tail = sb->tail;

#ifdef SILC_DIST_INPLACE
  SILC_ASSERT((sb->tail - len) >= sb->data);
#endif /* SILC_DIST_INPLACE */
  if (silc_unlikely((sb->tail - len) < sb->data)) {
    silc_set_errno(SILC_ERR_OVERFLOW);
    return NULL;
  }

  sb->tail -= len;
  return old_tail;
}

/****f* silcutil/silc_buffer_put_head
 *
 * SYNOPSIS
 *
 *    static inline
 *    unsigned char *silc_buffer_put_head(SilcBuffer sb,
 *					  const unsigned char *data,
 *					  SilcUInt32 len);
 *
 * DESCRIPTION
 *
 *    Puts data at the head of the buffer. Returns pointer to the copied
 *    data area. Returns NULL if the data is longer that the current head
 *    area.
 *
 * EXAMPLE
 *
 *    ---------------------------------
 *    | head  | data       | tail     |
 *    ---------------------------------
 *    ^
 *    Puts data to the head section.
 *
 *    silc_buffer_put_head(sb, data, data_len);
 *
 ***/

static inline
unsigned char *silc_buffer_put_head(SilcBuffer sb,
				    const unsigned char *data,
				    SilcUInt32 len)
{
#ifdef SILC_DIST_INPLACE
  SILC_ASSERT(len <= silc_buffer_headlen(sb));
#endif /* SILC_DIST_INPLACE */
  if (silc_unlikely(len > silc_buffer_headlen(sb))) {
    silc_set_errno(SILC_ERR_OVERFLOW);
    return NULL;
  }

  if (sb->head > data) {
    if (sb->head - data <= len)
      return (unsigned char *)memmove(sb->head, data, len);
  } else {
    if (data - sb->head <= len)
      return (unsigned char *)memmove(sb->head, data, len);
  }

  return (unsigned char *)memcpy(sb->head, data, len);
}

/****f* silcutil/silc_buffer_put
 *
 * SYNOPSIS
 *
 *    static inline
 *    unsigned char *silc_buffer_put(SilcBuffer sb,
 *				     const unsigned char *data,
 *				     SilcUInt32 len);
 *
 * DESCRIPTION
 *
 *    Puts data at the start of the valid data area. Returns a pointer
 *    to the copied data area.  Returns NULL if the data is longer than the
 *    current data area.
 *
 * EXAMPLE
 *
 *    ---------------------------------
 *    | head  | data       | tail     |
 *    ---------------------------------
 *            ^
 *            Puts data to the data section.
 *
 *    silc_buffer_put(sb, data, data_len);
 *
 ***/

static inline
unsigned char *silc_buffer_put(SilcBuffer sb,
			       const unsigned char *data,
			       SilcUInt32 len)
{
#ifdef SILC_DIST_INPLACE
  SILC_ASSERT(len <= silc_buffer_len(sb));
#endif /* SILC_DIST_INPLACE */
  if (silc_unlikely(len > silc_buffer_len(sb))) {
    silc_set_errno(SILC_ERR_OVERFLOW);
    return NULL;
  }

  if (sb->data > data) {
    if (sb->data - data <= len)
      return (unsigned char *)memmove(sb->data, data, len);
  } else {
    if (data - sb->data <= len)
      return (unsigned char *)memmove(sb->data, data, len);
  }

  return (unsigned char *)memcpy(sb->data, data, len);
}

/****f* silcutil/silc_buffer_put_tail
 *
 * SYNOPSIS
 *
 *    static inline
 *    unsigned char *silc_buffer_put_tail(SilcBuffer sb,
 *					  const unsigned char *data,
 *					  SilcUInt32 len);
 *
 * DESCRIPTION
 *
 *    Puts data at the tail of the buffer. Returns pointer to the copied
 *    data area.  Returns NULL if the data is longer than the current tail
 *    area.
 *
 * EXAMPLE
 *
 *    ---------------------------------
 *    | head  | data           | tail |
 *    ---------------------------------
 *                             ^
 * 			       Puts data to the tail section.
 *
 *    silc_buffer_put_tail(sb, data, data_len);
 *
 ***/

static inline
unsigned char *silc_buffer_put_tail(SilcBuffer sb,
				    const unsigned char *data,
				    SilcUInt32 len)
{
#ifdef SILC_DIST_INPLACE
  SILC_ASSERT(len <= silc_buffer_taillen(sb));
#endif /* SILC_DIST_INPLACE */
  if (silc_unlikely(len > silc_buffer_taillen(sb))) {
    silc_set_errno(SILC_ERR_OVERFLOW);
    return NULL;
  }

  if (sb->tail > data) {
    if (sb->tail - data <= len)
      return (unsigned char *)memmove(sb->tail, data, len);
  } else {
    if (data - sb->tail <= len)
      return (unsigned char *)memmove(sb->tail, data, len);
  }

  return (unsigned char *)memcpy(sb->tail, data, len);
}

/****f* silcutil/silc_buffer_alloc_size
 *
 * SYNOPSIS
 *
 *    static inline
 *    SilcBuffer silc_buffer_alloc_size(SilcUInt32 len);
 *
 * DESCRIPTION
 *
 *    Allocates `len' bytes size buffer and moves the tail area automatically
 *    `len' bytes so that the buffer is ready to use without calling the
 *    silc_buffer_pull_tail.  Returns NULL if system is out of memory.
 *
 ***/

static inline
SilcBuffer silc_buffer_alloc_size(SilcUInt32 len)
{
  SilcBuffer sb = silc_buffer_alloc(len);
  if (silc_unlikely(!sb))
    return NULL;
  silc_buffer_pull_tail(sb, len);
  return sb;
}

/****f* silcutil/silc_buffer_salloc_size
 *
 * SYNOPSIS
 *
 *    static inline
 *    SilcBuffer silc_buffer_salloc_size(SilcStack stack, SilcUInt32 len);
 *
 * DESCRIPTION
 *
 *    Allocates `len' bytes size buffer and moves the tail area automatically
 *    `len' bytes so that the buffer is ready to use without calling the
 *    silc_buffer_pull_tail.  Returns NULL if system is out of memory.
 *
 *    This routine use SilcStack are memory source.  If `stack' is NULL
 *    reverts back to normal allocating routine.
 *
 *    Note that this call consumes the `stack'.  The caller should push the
 *    stack before calling the function and pop it later.
 *
 ***/

static inline
SilcBuffer silc_buffer_salloc_size(SilcStack stack, SilcUInt32 len)
{
  SilcBuffer sb = silc_buffer_salloc(stack, len);
  if (silc_unlikely(!sb))
    return NULL;
  silc_buffer_pull_tail(sb, len);
  return sb;
}

/****f* silcutil/silc_buffer_reset
 *
 * SYNOPSIS
 *
 *    static inline
 *    void silc_buffer_reset(SilcBuffer sb);
 *
 * DESCRIPTION
 *
 *    Resets the buffer to the state as if it was just allocated by
 *    silc_buffer_alloc.  This does not clear the data area.  Use
 *    silc_buffer_clear if you also want to clear the data area.
 *
 ***/

static inline
void silc_buffer_reset(SilcBuffer sb)
{
  sb->data = sb->tail = sb->head;
}

/****f* silcutil/silc_buffer_clear
 *
 * SYNOPSIS
 *
 *    static inline
 *    void silc_buffer_clear(SilcBuffer sb);
 *
 * DESCRIPTION
 *
 *    Clears and initialiazes the buffer to the state as if it was just
 *    allocated by silc_buffer_alloc.
 *
 ***/

static inline
void silc_buffer_clear(SilcBuffer sb)
{
  memset(sb->head, 0, silc_buffer_truelen(sb));
  silc_buffer_reset(sb);
}

/****f* silcutil/silc_buffer_start
 *
 * SYNOPSIS
 *
 *    static inline
 *    void silc_buffer_start(SilcBuffer sb);
 *
 * DESCRIPTION
 *
 *    Moves the data area at the start of the buffer.  The tail area remains
 *    as is.
 *
 ***/

static inline
void silc_buffer_start(SilcBuffer sb)
{
  sb->data = sb->head;
}

/****f* silcutil/silc_buffer_end
 *
 * SYNOPSIS
 *
 *    static inline
 *    void silc_buffer_end(SilcBuffer sb);
 *
 * DESCRIPTION
 *
 *    Moves the end of the data area to the end of the buffer.  The start
 *    of the data area remains same.  If the start of data area is at the
 *    start of the buffer, after this function returns the buffer's data
 *    area length is the length of the entire buffer.
 *
 ***/

static inline
void silc_buffer_end(SilcBuffer sb)
{
  sb->tail = sb->end;
}

/****f* silcutil/silc_buffer_copy
 *
 * SYNOPSIS
 *
 *    static inline
 *    SilcBuffer silc_buffer_copy(SilcBuffer sb);
 *
 * DESCRIPTION
 *
 *    Generates copy of a SilcBuffer. This copies everything inside the
 *    currently valid data area, nothing more. Use silc_buffer_clone to
 *    copy entire buffer.  Returns NULL if system is out of memory.
 *
 ***/

static inline
SilcBuffer silc_buffer_copy(SilcBuffer sb)
{
  SilcBuffer sb_new;

  sb_new = silc_buffer_alloc_size(silc_buffer_len(sb));
  if (silc_unlikely(!sb_new))
    return NULL;
  silc_buffer_put(sb_new, sb->data, silc_buffer_len(sb));

  return sb_new;
}

/****f* silcutil/silc_buffer_scopy
 *
 * SYNOPSIS
 *
 *    static inline
 *    SilcBuffer silc_buffer_scopy(SilcStack stack, SilcBuffer sb);
 *
 * DESCRIPTION
 *
 *    Generates copy of a SilcBuffer. This copies everything inside the
 *    currently valid data area, nothing more. Use silc_buffer_clone to
 *    copy entire buffer.  Returns NULL if system is out of memory.
 *
 *    This routine use SilcStack are memory source.  If `stack' is NULL
 *    reverts back to normal allocating routine.
 *
 *    Note that this call consumes the `stack'.  The caller should push the
 *    stack before calling the function and pop it later.
 *
 ***/

static inline
SilcBuffer silc_buffer_scopy(SilcStack stack, SilcBuffer sb)
{
  SilcBuffer sb_new;

  sb_new = silc_buffer_salloc_size(stack, silc_buffer_len(sb));
  if (silc_unlikely(!sb_new))
    return NULL;
  silc_buffer_put(sb_new, sb->data, silc_buffer_len(sb));

  return sb_new;
}

/****f* silcutil/silc_buffer_clone
 *
 * SYNOPSIS
 *
 *    static inline
 *    SilcBuffer silc_buffer_clone(SilcBuffer sb);
 *
 * DESCRIPTION
 *
 *    Clones SilcBuffer. This generates new SilcBuffer and copies
 *    everything from the source buffer. The result is exact clone of
 *    the original buffer.  Returns NULL if system is out of memory.
 *
 ***/

static inline
SilcBuffer silc_buffer_clone(SilcBuffer sb)
{
  SilcBuffer sb_new;

  sb_new = silc_buffer_alloc_size(silc_buffer_truelen(sb));
  if (silc_unlikely(!sb_new))
    return NULL;
  silc_buffer_put(sb_new, sb->head, silc_buffer_truelen(sb));
  sb_new->data = sb_new->head + silc_buffer_headlen(sb);
  sb_new->tail = sb_new->data + silc_buffer_len(sb);

  return sb_new;
}

/****f* silcutil/silc_buffer_sclone
 *
 * SYNOPSIS
 *
 *    static inline
 *    SilcBuffer silc_buffer_sclone(SilcStack stack, SilcBuffer sb);
 *
 * DESCRIPTION
 *
 *    Clones SilcBuffer. This generates new SilcBuffer and copies
 *    everything from the source buffer. The result is exact clone of
 *    the original buffer.  Returns NULL if system is out of memory.
 *
 *    This routine use SilcStack are memory source.  If `stack' is NULL
 *    reverts back to normal allocating routine.
 *
 *    Note that this call consumes the `stack'.  The caller should push the
 *    stack before calling the function and pop it later.
 *
 ***/

static inline
SilcBuffer silc_buffer_sclone(SilcStack stack, SilcBuffer sb)
{
  SilcBuffer sb_new;

  sb_new = silc_buffer_salloc_size(stack, silc_buffer_truelen(sb));
  if (silc_unlikely(!sb_new))
    return NULL;
  silc_buffer_put(sb_new, sb->head, silc_buffer_truelen(sb));
  sb_new->data = sb_new->head + silc_buffer_headlen(sb);
  sb_new->tail = sb_new->data + silc_buffer_len(sb);

  return sb_new;
}

/****f* silcutil/silc_buffer_realloc
 *
 * SYNOPSIS
 *
 *    static inline
 *    SilcBuffer silc_buffer_realloc(SilcBuffer sb, SilcUInt32 newsize);
 *
 * DESCRIPTION
 *
 *    Reallocates buffer. Old data is saved into the new buffer. The buffer
 *    is exact clone of the old one except that there is now more/less space
 *    at the end of buffer.  This always returns the same `sb' unless `sb'
 *    was NULL. Returns NULL if system is out of memory.
 *
 *    If the `newsize' is shorter than the current buffer size, the data
 *    and tail area of the buffer must be set to correct position before
 *    calling this function so that buffer overflow would not occur when
 *    the buffer size is reduced.
 *
 ***/

static inline
SilcBuffer silc_buffer_realloc(SilcBuffer sb, SilcUInt32 newsize)
{
  SilcUInt32 hlen, dlen;
  unsigned char *h;

  if (!sb)
    return silc_buffer_alloc(newsize);

  if (silc_unlikely(newsize == silc_buffer_truelen(sb)))
    return sb;

  hlen = silc_buffer_headlen(sb);
  dlen = silc_buffer_len(sb);
  h = (unsigned char *)silc_realloc(sb->head, newsize);
  if (silc_unlikely(!h))
    return NULL;
  sb->head = h;
  sb->data = sb->head + hlen;
  sb->tail = sb->data + dlen;
  sb->end = sb->head + newsize;

  return sb;
}

/****f* silcutil/silc_buffer_srealloc
 *
 * SYNOPSIS
 *
 *    static inline
 *    SilcBuffer silc_buffer_srealloc(SilcStack stack,
 *                                    SilcBuffer sb, SilcUInt32 newsize);
 *
 * DESCRIPTION
 *
 *    Reallocates buffer. Old data is saved into the new buffer. The buffer
 *    is exact clone of the old one except that there is now more/less space
 *    at the end of buffer.  Returns NULL if system is out of memory.  This
 *    always returns `sb' unless `sb' was NULL.
 *
 *    If the `newsize' is shorter than the current buffer size, the data
 *    and tail area of the buffer must be set to correct position before
 *    calling this function so that buffer overflow would not occur when
 *    the buffer size is reduced.
 *
 *    This routine use SilcStack are memory source.  If `stack' is NULL
 *    reverts back to normal allocating routine.
 *
 *    Note that this call consumes the `stack'.  The caller should push the
 *    stack before calling the function and pop it later.
 *
 ***/

static inline
SilcBuffer silc_buffer_srealloc(SilcStack stack,
				SilcBuffer sb, SilcUInt32 newsize)
{
  SilcUInt32 hlen, dlen;
  unsigned char *h;

  if (!stack)
    return silc_buffer_realloc(sb, newsize);

  if (!sb)
    return silc_buffer_salloc(stack, newsize);

  if (newsize == silc_buffer_truelen(sb))
    return sb;

  hlen = silc_buffer_headlen(sb);
  dlen = silc_buffer_len(sb);
  h = (unsigned char *)silc_srealloc(stack, silc_buffer_truelen(sb),
				     sb->head, newsize);
  if (!h)
    return NULL;

  sb->head = h;
  sb->data = sb->head + hlen;
  sb->tail = sb->data + dlen;
  sb->end = sb->head + newsize;

  return sb;
}

/****f* silcutil/silc_buffer_realloc_size
 *
 * SYNOPSIS
 *
 *    static inline
 *    SilcBuffer silc_buffer_realloc_size(SilcBuffer sb, SilcUInt32 newsize);
 *
 * DESCRIPTION
 *
 *    Same as silc_buffer_realloc but moves moves the tail area
 *    automatically so that the buffer is ready to use without calling the
 *    silc_buffer_pull_tail.  Returns NULL if system is out of memory.
 *
 ***/

static inline
SilcBuffer silc_buffer_realloc_size(SilcBuffer sb, SilcUInt32 newsize)
{
  sb = silc_buffer_realloc(sb, newsize);
  if (silc_unlikely(!sb))
    return NULL;
  silc_buffer_pull_tail(sb, silc_buffer_taillen(sb));
  return sb;
}

/****f* silcutil/silc_buffer_srealloc_size
 *
 * SYNOPSIS
 *
 *    static inline
 *    SilcBuffer silc_buffer_srealloc_size(SilcStack stack,
 *                                         SilcBuffer sb, SilcUInt32 newsize);
 *
 * DESCRIPTION
 *
 *    Same as silc_buffer_srealloc but moves moves the tail area
 *    automatically so that the buffer is ready to use without calling the
 *    silc_buffer_pull_tail.  Returns NULL if system is out of memory.
 *
 *    This routine use SilcStack are memory source.  If `stack' is NULL
 *    reverts back to normal allocating routine.
 *
 *    Note that this call consumes the `stack'.  The caller should push the
 *    stack before calling the function and pop it later.
 *
 ***/

static inline
SilcBuffer silc_buffer_srealloc_size(SilcStack stack,
				     SilcBuffer sb, SilcUInt32 newsize)
{
  sb = silc_buffer_srealloc(stack, sb, newsize);
  if (silc_unlikely(!sb))
    return NULL;
  silc_buffer_pull_tail(sb, silc_buffer_taillen(sb));
  return sb;
}

/****f* silcutil/silc_buffer_enlarge
 *
 * SYNOPSIS
 *
 *    static inline
 *    SilcBuffer silc_buffer_enlarge(SilcBuffer sb, SilcUInt32 size);
 *
 * DESCRIPTION
 *
 *    Enlarges the buffer by the amount of `size' if it doesn't have that
 *    must space in the data area and in the tail area.  Moves the tail
 *    area automatically after enlarging so that the current data area
 *    is at least the size of `size'.  If there is more space than `size'
 *    in the data area this does not do anything.  If there is enough
 *    space in the tail area this merely moves the tail area to reveal
 *    the extra space.  Returns FALSE if system is out of memory.
 *
 ***/

static inline
SilcBool silc_buffer_enlarge(SilcBuffer sb, SilcUInt32 size)
{
  if (size > silc_buffer_len(sb)) {
    if (size > silc_buffer_taillen(sb) + silc_buffer_len(sb))
      if (silc_unlikely(!silc_buffer_realloc(sb, silc_buffer_truelen(sb) +
					     (size - silc_buffer_taillen(sb) -
					      silc_buffer_len(sb)))))
	return FALSE;
    silc_buffer_pull_tail(sb, size - silc_buffer_len(sb));
  }
  return TRUE;
}

/****f* silcutil/silc_buffer_senlarge
 *
 * SYNOPSIS
 *
 *    static inline
 *    SilcBuffer silc_buffer_senlarge(SilcStack stack, SilcBuffer sb,
 *                                    SilcUInt32 size);
 *
 * DESCRIPTION
 *
 *    Enlarges the buffer by the amount of `size' if it doesn't have that
 *    must space in the data area and in the tail area.  Moves the tail
 *    area automatically after enlarging so that the current data area
 *    is at least the size of `size'.  If there is more space than `size'
 *    in the data area this does not do anything.  If there is enough
 *    space in the tail area this merely moves the tail area to reveal
 *    the extra space.  Returns FALSE if system is out of memory.
 *
 *    This routine use SilcStack are memory source.  If `stack' is NULL
 *    reverts back to normal allocating routine.
 *
 *    Note that this call consumes the `stack'.  The caller should push the
 *    stack before calling the function and pop it later.
 *
 ***/

static inline
SilcBool silc_buffer_senlarge(SilcStack stack, SilcBuffer sb, SilcUInt32 size)
{
  if (size > silc_buffer_len(sb)) {
    if (size > silc_buffer_taillen(sb) + silc_buffer_len(sb))
      if (silc_unlikely(!silc_buffer_srealloc(stack, sb,
					      silc_buffer_truelen(sb) +
					      (size - silc_buffer_taillen(sb) -
					       silc_buffer_len(sb)))))
	return FALSE;
    silc_buffer_pull_tail(sb, size - silc_buffer_len(sb));
  }
  return TRUE;
}

/****f* silcutil/silc_buffer_append
 *
 * SYNOPSIS
 *
 *    static inline
 *    SilcBuffer silc_buffer_append(SilcBuffer sb, SilcUInt32 size);
 *
 * DESCRIPTION
 *
 *    Appends the current data area by the amount of `size'.  The tail area
 *    of the buffer remains intact and contains the same data than the old
 *    tail area (the data is copied to the new tail area).  After appending
 *    there is now `size' bytes more free area in the data area.  Returns
 *    FALSE if system is out of memory.
 *
 * EXAMPLE
 *
 *    Before appending:
 *    ---------------------------------
 *    | head  | data           | tail |
 *    ---------------------------------
 *
 *    After appending:
 *    ------------------------------------
 *    | head  | data               | tail |
 *    -------------------------------------
 *
 *    silc_buffer_append(sb, 5);
 *
 ***/

static inline
SilcBool silc_buffer_append(SilcBuffer sb, SilcUInt32 size)
{
  if (silc_unlikely(!silc_buffer_realloc(sb, silc_buffer_truelen(sb) + size)))
    return FALSE;

  /* Enlarge data area */
  silc_buffer_pull_tail(sb, size);

  /* Copy old tail area to new tail area */
  silc_buffer_put_tail(sb, sb->tail - size, silc_buffer_taillen(sb));

  return TRUE;
}

/****f* silcutil/silc_buffer_sappend
 *
 * SYNOPSIS
 *
 *    static inline
 *    SilcBool silc_buffer_sappend(SilcStack stack, SilcBuffer sb,
 *                                 SilcUInt32 size)
 *
 * DESCRIPTION
 *
 *    Appends the current data area by the amount of `size'.  The tail area
 *    of the buffer remains intact and contains the same data than the old
 *    tail area (the data is copied to the new tail area).  After appending
 *    there is now `size' bytes more free area in the data area.  Returns
 *    FALSE if system is out of memory.
 *
 *    This routine use SilcStack are memory source.  If `stack' is NULL
 *    reverts back to normal allocating routine.
 *
 *    Note that this call consumes the `stack'.  The caller should push the
 *    stack before calling the function and pop it later.
 *
 * EXAMPLE
 *
 *    Before appending:
 *    ---------------------------------
 *    | head  | data           | tail |
 *    ---------------------------------
 *
 *    After appending:
 *    ------------------------------------
 *    | head  | data               | tail |
 *    -------------------------------------
 *
 *    silc_buffer_append(sb, 5);
 *
 ***/

static inline
SilcBool silc_buffer_sappend(SilcStack stack, SilcBuffer sb, SilcUInt32 size)
{
  if (silc_unlikely(!silc_buffer_srealloc(stack, sb,
					  silc_buffer_truelen(sb) + size)))
    return FALSE;

  /* Enlarge data area */
  silc_buffer_pull_tail(sb, size);

  /* Copy old tail area to new tail area */
  silc_buffer_put_tail(sb, sb->tail - size, silc_buffer_taillen(sb));

  return TRUE;
}

/****f* silcutil/silc_buffer_strchr
 *
 * SYNOPSIS
 *
 *    static inline
 *    unsigned char *silc_buffer_strchr(SilcBuffer sb, int c, SilcBool first);
 *
 * DESCRIPTION
 *
 *    Returns pointer to the occurence of the character `c' in the buffer
 *    `sb'.  If the `first' is TRUE this finds the first occurene of `c',
 *    if it is FALSE this finds the last occurence of `c'.  If the character
 *    is found the `sb' data area is moved to that location and its pointer
 *    is returned.  The silc_buffer_data call will return the same pointer.
 *    Returns NULL if such character could not be located and the buffer
 *    remains unmodified.
 *
 *    This call is equivalent to strchr(), strrchr(), memchr() and memrchr()
 *    except it works with SilcBuffer.
 *
 * NOTES
 *
 *    This searches only the data area of the buffer.  Head and tail area
 *    are not searched.
 *
 *    The `sb' data need not be NULL terminated.
 *
 ***/

static inline
unsigned char *silc_buffer_strchr(SilcBuffer sb, int c, SilcBool first)
{
  int i;

  if (first) {
    for (i = 0; i < silc_buffer_len(sb); i++) {
      if (sb->data[i] == (unsigned char)c) {
	sb->data = &sb->data[i];
	return sb->data;
      }
    }
  } else {
    for (i = silc_buffer_len(sb) - 1; 1 >= 0; i--) {
      if (sb->data[i] == (unsigned char)c) {
	sb->data = &sb->data[i];
	return sb->data;
      }
    }
  }

  return NULL;
}

/****f* silcutil/silc_buffer_equal
 *
 * SYNOPSIS
 *
 *    static inline
 *    SilcBool silc_buffer_equal(SilcBuffer sb1, SilcBuffer sb2)
 *
 * DESCRIPTION
 *
 *    Compares if the data area of the buffer `sb1' and `sb2' are identical.
 *    Returns TRUE if they match and FALSE if they differ.
 *
 ***/

static inline
SilcBool silc_buffer_equal(SilcBuffer sb1, SilcBuffer sb2)
{
  if (silc_buffer_len(sb1) != silc_buffer_len(sb2))
    return FALSE;
  return memcmp(sb1->data, sb2->data, silc_buffer_len(sb1)) == 0;
}

/****f* silcutil/silc_buffer_memcmp
 *
 * SYNOPSIS
 *
 *    static inline
 *    SilcBool silc_buffer_memcmp(SilcBuffer buffer,
 *                                const unsigned char *data,
 *                                SilcUInt32 data_len)
 *
 * DESCRIPTION
 *
 *    Compares the data area of the buffer with the `data'.  Returns TRUE
 *    if the data area is identical to `data' or FALSE if they differ.
 *
 ***/

static inline
SilcBool silc_buffer_memcmp(SilcBuffer buffer, const unsigned char *data,
			    SilcUInt32 data_len)
{
  if (silc_buffer_len(buffer) != data_len)
    return FALSE;
  return memcmp(buffer->data, data, data_len) == 0;
}

/****f* silcutil/silc_buffer_printf
 *
 * SYNOPSIS
 *
 *    static inline
 *    void silc_buffer_printf(SilcBuffer sb, SilcBool newline);
 *
 * DESCRIPTION
 *
 *    Prints the current data area of `sb' into stdout.  If `newline' is
 *    TRUE prints '\n' after the data in the buffer.
 *
 ***/

static inline
void silc_buffer_printf(SilcBuffer sb, SilcBool newline)
{
  silc_file_write(1, (const char *)silc_buffer_data(sb),
		  silc_buffer_len(sb));
  if (newline)
    printf("\n");
  fflush(stdout);
}

#endif /* SILCBUFFER_H */
