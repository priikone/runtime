/*

  silcfileutil.c

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

#include "silcruntime.h"

/* Opens a file indicated by the filename `filename' with flags indicated
   by the `flags'. */

int silc_file_open(const char *filename, int flags)
{
  return silc_file_open_mode(filename, flags, 0600);
}

/* Opens a file indicated by the filename `filename' with flags indicated
   by the `flags', and with the specified `mode'. */

int silc_file_open_mode(const char *filename, int flags, int mode)
{
  int fd = open(filename, flags, mode);
  if (fd < 0)
    silc_set_errno_posix(errno);
  return fd;
}

/* Reads data from file descriptor `fd' to `buf'. */

int silc_file_read(int fd, unsigned char *buf, SilcUInt32 buf_len)
{
  int ret = read(fd, (void *)buf, buf_len);
  if (ret < 0)
    silc_set_errno_posix(errno);
  return ret;
}

/* Writes `buffer' of length of `len' to file descriptor `fd'. */

int silc_file_write(int fd, const char *buffer, SilcUInt32 len)
{
  int ret = write(fd, (const void *)buffer, len);
  if (ret < 0)
    silc_set_errno_posix(errno);
  return ret;
}

/* Closes file descriptor */

int silc_file_close(int fd)
{
  int ret = close(fd);
  if (ret < 0)
    silc_set_errno_posix(errno);
  return ret;
}

/* Writes a buffer to the file. */

int silc_file_writefile(const char *filename, const char *buffer,
			SilcUInt32 len)
{
  int fd;
  int flags = O_CREAT | O_WRONLY | O_TRUNC;

#if defined(O_BINARY)
  flags |= O_BINARY;
#endif /* O_BINARY */

  if ((fd = open(filename, flags, 0644)) == -1) {
    SILC_LOG_ERROR(("Cannot open file %s for writing: %s", filename,
		    silc_errno_string(silc_errno)));
    return -1;
  }

  if (silc_file_write(fd, buffer, len) == -1) {
    SILC_LOG_ERROR(("Cannot write to file %s: %s", filename,
		    silc_errno_string(silc_errno)));
    silc_file_close(fd);
    return -1;
  }

#ifdef SILC_UNIX
  fsync(fd);
#endif /* SILC_UNIX */

  return silc_file_close(fd);
}

/* Writes a buffer to the file.  If the file is created specific mode is
   set to the file. */

int silc_file_writefile_mode(const char *filename, const char *buffer,
			     SilcUInt32 len, int mode)
{
  int fd;
  int flags = O_CREAT | O_WRONLY | O_TRUNC;

#if defined(O_BINARY)
  flags |= O_BINARY;
#endif /* O_BINARY */

  if ((fd = open(filename, flags, mode)) == -1) {
    SILC_LOG_ERROR(("Cannot open file %s for writing: %s", filename,
		    silc_errno_string(silc_errno)));
    return -1;
  }

  if ((silc_file_write(fd, buffer, len)) == -1) {
    SILC_LOG_ERROR(("Cannot write to file %s: %s", filename,
		    silc_errno_string(silc_errno)));
    silc_file_close(fd);
    return -1;
  }

#ifdef SILC_UNIX
  fsync(fd);
#endif /* SILC_UNIX */

  return silc_file_close(fd);
}

/* Reads a file to a buffer. The allocated buffer is returned. Length of
   the file read is returned to the return_len argument. */

char *silc_file_readfile(const char *filename, SilcUInt32 *return_len,
			 SilcStack stack)
{
  int fd;
  unsigned char *buffer;
  int filelen;

  fd = silc_file_open(filename, O_RDONLY);
  if (fd < 0) {
    if (silc_errno == SILC_ERR_NO_SUCH_FILE)
      return NULL;
    SILC_LOG_ERROR(("Cannot open file %s: %s", filename,
		    silc_errno_string(silc_errno)));
    return NULL;
  }

  filelen = lseek(fd, (off_t)0L, SEEK_END);
  if (filelen < 0) {
    silc_set_errno_posix(errno);
    silc_file_close(fd);
    return NULL;
  }
  if (lseek(fd, (off_t)0L, SEEK_SET) < 0) {
    silc_set_errno_posix(errno);
    silc_file_close(fd);
    return NULL;
  }

  buffer = silc_calloc(filelen + 1, sizeof(*buffer));

  if ((silc_file_read(fd, buffer, filelen)) == -1) {
    memset(buffer, 0, sizeof(buffer));
    silc_file_close(fd);
    SILC_LOG_ERROR(("Cannot read from file %s: %s", filename,
		    silc_errno_string(silc_errno)));
    return NULL;
  }

  silc_file_close(fd);
  buffer[filelen] = EOF;

  if (return_len)
    *return_len = filelen;

  return (char *)buffer;
}

/* Returns the size of `filename'. Returns 0 on error. */

SilcUInt64 silc_file_size(const char *filename)
{
  int ret;
  struct stat stats;

#ifdef SILC_WIN32
  ret = stat(filename, &stats);
#endif /* SILC_WIN32 */
#ifdef SILC_UNIX
  ret = lstat(filename, &stats);
#endif /* SILC_UNIX */
#ifdef SILC_SYMBIAN
  ret = stat(filename, &stats);
#endif /* SILC_SYMBIAN */
  if (ret < 0) {
    silc_set_errno_posix(errno);
    return 0;
  }

  return (SilcUInt64)stats.st_size;
}
