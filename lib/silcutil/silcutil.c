/*

  silcutil.c

  Author: Pekka Riikonen <priikone@silcnet.org>

  Copyright (C) 1997 - 2008 Pekka Riikonen

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

*/
/*
 * These are general utility functions that doesn't belong to any specific
 * group of routines.
 */

#include "silcruntime.h"

/* Gets line from a buffer. Stops reading when a newline or EOF occurs.
   This doesn't remove the newline sign from the destination buffer. The
   argument begin is returned and should be passed again for the function. */

int silc_gets(char *dest, int destlen, const char *src, int srclen, int begin)
{
  static int start = 0;
  int i;

  memset(dest, 0, destlen);

  if (begin != start)
    start = 0;

  i = 0;
  for ( ; start <= srclen; i++, start++) {
    if (i > destlen) {
      silc_set_errno(SILC_ERR_OVERFLOW);
      return -1;
    }

    dest[i] = src[start];

    if (dest[i] == EOF) {
      silc_set_errno(SILC_ERR_EOF);
      return EOF;
    }

    if (dest[i] == '\n')
      break;
  }
  start++;

  return start;
}

/* Converts string to capital characters. */

SilcBool silc_to_upper(const char *string, char *dest, SilcUInt32 dest_size)
{
  int i;

  if (strlen(string) > dest_size) {
    silc_set_errno(SILC_ERR_OVERFLOW);
    return FALSE;
  }

  for (i = 0; i < strlen(string); i++)
    dest[i] = (char)toupper((int)string[i]);

  return TRUE;
}

/* Converts string to lower letter characters. */

SilcBool silc_to_lower(const char *string, char *dest, SilcUInt32 dest_size)
{
  int i;

  if (strlen(string) > dest_size) {
    silc_set_errno(SILC_ERR_OVERFLOW);
    return FALSE;
  }

  for (i = 0; i < strlen(string); i++)
    dest[i] = (char)tolower((int)string[i]);

  return TRUE;
}

/* Parse userfqdn string which is in user@fqdn format. */

int silc_parse_userfqdn(const char *string,
			char *user, SilcUInt32 user_size,
			char *fqdn, SilcUInt32 fqdn_size)
{
  SilcUInt32 tlen;

  if (!user && !fqdn) {
    silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
    return 0;
  }

  if (user)
    memset(user, 0, user_size);
  if (fqdn)
    memset(fqdn, 0, fqdn_size);

  if (!string) {
    silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
    return 0;
  }

  if (string[0] == '@') {
    if (user)
      silc_strncat(user, user_size, string, strlen(string));

    return 1;
  }

  if (strchr(string, '@')) {
    tlen = strcspn(string, "@");

    if (user)
      silc_strncat(user, user_size, string, tlen);

    if (fqdn)
      silc_strncat(fqdn, fqdn_size, string + tlen + 1,
		   strlen(string) - tlen - 1);

    return 2;
  }

  if (user)
    silc_strncat(user, user_size, string, strlen(string));

  return 1;
}

/* Parses command line. At most `max_args' is taken. Rest of the line
   will be allocated as the last argument if there are more than `max_args'
   arguments in the line. Note that the command name is counted as one
   argument and is saved. */

void silc_parse_command_line(unsigned char *buffer,
			     unsigned char ***parsed,
			     SilcUInt32 **parsed_lens,
			     SilcUInt32 **parsed_types,
			     SilcUInt32 *parsed_num,
			     SilcUInt32 max_args)
{
  int i, len = 0;
  int argc = 0;
  const char *cp = (const char *)buffer;
  char *tmp;

  *parsed = silc_calloc(1, sizeof(**parsed));
  *parsed_lens = silc_calloc(1, sizeof(**parsed_lens));

  /* Get the command first */
  len = strcspn(cp, " ");
  tmp = silc_calloc(strlen(cp) + 1, sizeof(*tmp));
  if (!tmp)
    return;
  silc_to_upper(cp, tmp, strlen(cp));
  (*parsed)[0] = silc_calloc(len + 1, sizeof(char));
  memcpy((*parsed)[0], tmp, len);
  silc_free(tmp);
  (*parsed_lens)[0] = len;
  cp += len;
  while (*cp == ' ')
    cp++;
  argc++;

  /* Parse arguments */
  if (strchr(cp, ' ') || strlen(cp) != 0) {
    for (i = 1; i < max_args; i++) {

      if (i != max_args - 1)
	len = strcspn(cp, " ");
      else
	len = strlen(cp);
      while (len && cp[len - 1] == ' ')
	len--;
      if (!len)
	break;

      *parsed = silc_realloc(*parsed, sizeof(**parsed) * (argc + 1));
      *parsed_lens = silc_realloc(*parsed_lens,
				  sizeof(**parsed_lens) * (argc + 1));
      (*parsed)[argc] = silc_calloc(len + 1, sizeof(char));
      memcpy((*parsed)[argc], cp, len);
      (*parsed_lens)[argc] = len;
      argc++;

      cp += len;
      if (strlen(cp) == 0)
	break;
      else
	while (*cp == ' ')
	  cp++;
    }
  }

  /* Save argument types. Protocol defines all argument types but
     this implementation makes sure that they are always in correct
     order hence this simple code. */
  *parsed_types = silc_calloc(argc, sizeof(**parsed_types));
  for (i = 0; i < argc; i++)
    (*parsed_types)[i] = i;

  *parsed_num = argc;
}

/* Formats arguments to a string and returns it after allocating memory
   for it. It must be remembered to free it later. */

char *silc_format(char *fmt, ...)
{
  va_list args;
  char buf[8192];

  va_start(args, fmt);
  silc_vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  return silc_strdup(buf);
}

/* Creates fingerprint from data, usually used with SHA1 digests */

char *silc_fingerprint(const unsigned char *data, SilcUInt32 data_len)
{
  unsigned char *fingerprint, *cp;
  unsigned int len, blocks, i;

  if (!data || !data_len) {
    silc_set_errno(SILC_ERR_INVALID_ARGUMENT);
    return NULL;
  }

  if (data_len >= 256)
    data_len = 255;

  /* Align and calculate total length */
  len = ((data_len + 19) / 20) * 20;
  blocks = (len / 10);
  len = (len * 2) + ((blocks - 1) * 2) + (4 * blocks) + 2 + 1;

  cp = fingerprint = silc_calloc(len, sizeof(*fingerprint));
  if (!cp)
    return NULL;

  for (i = 0; i < data_len; i++) {
    silc_snprintf((char *)cp, len, "%02X", data[i]);
    cp += 2;
    len -= 2;

    if ((i + 1) % 2 == 0)
      silc_snprintf((char *)cp++, len--, " ");
    if ((i + 1) % 10 == 0)
      silc_snprintf((char *)cp++, len--, " ");
  }
  i--;
  if ((i + 1) % 10 == 0)
    *(--cp) = '\0';
  if ((i + 1) % 2 == 0)
    *(--cp) = '\0';

  return (char *)fingerprint;
}

/* Return TRUE if the `data' is ASCII string. */

SilcBool silc_string_is_ascii(const unsigned char *data, SilcUInt32 data_len)
{
  int i;

  for (i = 0; i < data_len; i++) {
    if (!isascii(data[i]))
      return FALSE;
  }

  return TRUE;
}

/* Displays input prompt on command line and takes input data from user */

char *silc_get_input(const char *prompt, SilcBool echo_off)
{
#ifdef SILC_UNIX
  int fd;
  char input[2048];

  if (echo_off) {
    char *ret = NULL;
#ifdef HAVE_TERMIOS_H
    struct termios to;
    struct termios to_old;

    fd = open("/dev/tty", O_RDONLY);
    if (fd < 0) {
      silc_set_errno_posix(errno);
      return NULL;
    }

    signal(SIGINT, SIG_IGN);

    /* Get terminal info */
    tcgetattr(fd, &to);
    to_old = to;

    /* Echo OFF, and assure we can prompt and get input */
    to.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
    to.c_lflag |= ICANON;
    to.c_cc[VMIN] = 255;
    tcsetattr(fd, TCSANOW, &to);

    memset(input, 0, sizeof(input));

    printf("%s", prompt);
    fflush(stdout);

    if ((read(fd, input, sizeof(input))) < 0) {
      silc_set_errno_posix(errno);
      tcsetattr(fd, TCSANOW, &to_old);
      return NULL;
    }

    if (strlen(input) <= 1) {
      tcsetattr(fd, TCSANOW, &to_old);
      silc_set_errno(SILC_ERR_EOF);
      return NULL;
    }

    if (strchr(input, '\n'))
      *strchr(input, '\n') = '\0';

    /* Restore old terminfo */
    tcsetattr(fd, TCSANOW, &to_old);
    signal(SIGINT, SIG_DFL);

    ret = silc_memdup(input, strlen(input));
    memset(input, 0, sizeof(input));
#endif /* HAVE_TERMIOS_H */
    return ret;
  } else {
    fd = open("/dev/tty", O_RDONLY);
    if (fd < 0) {
      silc_set_errno_posix(errno);
      return NULL;
    }

    memset(input, 0, sizeof(input));

    printf("%s", prompt);
    fflush(stdout);

    if ((read(fd, input, sizeof(input))) < 0) {
      silc_set_errno_posix(errno);
      return NULL;
    }

    if (strlen(input) <= 1) {
      silc_set_errno(SILC_ERR_EOF);
      return NULL;
    }

    if (strchr(input, '\n'))
      *strchr(input, '\n') = '\0';

    return silc_strdup(input);
  }
#else
  return NULL;
#endif /* SILC_UNIX */
}

/* Hexdump */

void silc_hexdump(const unsigned char *data, SilcUInt32 data_len,
		  FILE *output)
{
  int i, k;
  int off, pos, count;
  int len = data_len;

  k = 0;
  pos = 0;
  count = 16;
  off = len % 16;
  while (1) {
    if (off) {
      if ((len - pos) < 16 && (len - pos <= len - off))
	count = off;
    } else {
      if (pos == len)
	count = 0;
    }
    if (off == len)
      count = len;

    if (count)
      fprintf(output, "%08X  ", k++ * 16);

    for (i = 0; i < count; i++) {
      fprintf(output, "%02X ", data[pos + i]);

      if ((i + 1) % 4 == 0)
	fprintf(output, " ");
    }

    if (count && count < 16) {
      int j;

      for (j = 0; j < 16 - count; j++) {
	fprintf(output, "   ");

	if ((j + count + 1) % 4 == 0)
	  fprintf(output, " ");
      }
    }

    for (i = 0; i < count; i++) {
      char ch;

      if (data[pos] < 32 || data[pos] >= 127)
	ch = '.';
      else
	ch = data[pos];

      fprintf(output, "%c", ch);
      pos++;
    }

    if (count)
      fprintf(output, "\n");

    if (count < 16)
      break;
  }
}

/* Convert hex string to data.  Each hex number must have two characters. */

SilcBool silc_hex2data(const char *hex, unsigned char *data,
		       SilcUInt32 data_size, SilcUInt32 *ret_data_len)
{
  char *cp = (char *)hex;
  unsigned char l, h;
  int i;

  if (data_size < strlen(hex) / 2) {
    silc_set_errno(SILC_ERR_OVERFLOW);
    return FALSE;
  }

  for (i = 0; i < strlen(hex) / 2; i++) {
    h = *cp++;
    l = *cp++;

    h -= h < 'A' ? '0' : 'A' - 10;
    l -= l < 'A' ? '0' : 'A' - 10;

    data[i] = (h << 4) | (l & 0xf);
  }

  if (ret_data_len)
    *ret_data_len = i;

  return TRUE;
}

/* Converts binary data to HEX string */

SilcBool silc_data2hex(const unsigned char *data, SilcUInt32 data_len,
		       char *hex, SilcUInt32 hex_size)
{
  unsigned char l, h;
  char *cp = hex;
  int i;

  if (hex_size - 1 < data_len * 2) {
    silc_set_errno(SILC_ERR_OVERFLOW);
    return FALSE;
  }

  memset(hex, 0, hex_size);

  for (i = 0; i < data_len; i++) {
    l = data[i];
    h = l >> 4;
    l &= 0xf;

    *cp++ = h + (h > 9 ? 'A' - 10 : '0');
    *cp++ = l + (l > 9 ? 'A' - 10 : '0');
  }

  return TRUE;
}
