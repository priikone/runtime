/*

  silcfdstream.h

  Author: Pekka Riikonen <priikone@silcnet.org>

  Copyright (C) 2005 - 2008 Pekka Riikonen

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

*/

/****h* silcutil/Fd Stream Interface
 *
 * DESCRIPTION
 *
 * Implementation of SILC File Descriptor Stream.  The file descriptor
 * stream can be used read from and write to a file descriptor.  This
 * interface should be used only with real file descriptors, not with
 * sockets.  Use the SILC Socket Stream for sockets.
 *
 * SILC File Descriptor Stream is not thread-safe.  If same stream must be
 * used in multithreaded environment concurrency control must be employed.
 *
 ***/

#ifndef SILCFDSTREAM_H
#define SILCFDSTREAM_H

/****f* silcutil/silc_fd_stream_create
 *
 * SYNOPSIS
 *
 *    SilcStream silc_fd_stream_create(int fd, SilcStack stack);
 *
 * DESCRIPTION
 *
 *    Creates file descriptor stream for the open file descriptor indicated
 *    by `fd'.  The stream is closed with the silc_stream_close and destroyed
 *    with the silc_stream_destroy.  Returns NULL if system is out of memory.
 *
 *    If the silc_stream_set_notifier is called the stream is set to
 *    non-blocking mode.
 *
 *    If `stack' is non-NULL all memory is allocated from the `stack' and
 *    will be released back to `stack' after the stream is destroyed.
 *
 ***/
SilcStream silc_fd_stream_create(int fd, SilcStack stack);

/****f* silcutil/silc_fd_stream_create2
 *
 * SYNOPSIS
 *
 *    SilcStream silc_fd_stream_create2(int read_fd, int write_fd,
 *                                      SilcStack stack);
 *
 * DESCRIPTION
 *
 *    Creates file descriptor stream for the open file descriptors indicated
 *    by `read_fd' and `write_fd'.  The `read_fd' must be opened for reading
 *    and `write_fd' opened for writing.  The stream is closed with the
 *    silc_stream_close and destroyed with the silc_stream_destroy.  Returns
 *    NULL if system is out of memory.
 *
 *    If the silc_stream_set_notifier is called the stream is set to
 *    non-blocking mode.
 *
 *    If `stack' is non-NULL all memory is allocated from the `stack' and
 *    will be released back to `stack' after the stream is destroyed.
 *
 ***/
SilcStream silc_fd_stream_create2(int read_fd, int write_fd, SilcStack stack);

/****f* silcutil/silc_fd_stream_file
 *
 * SYNOPSIS
 *
 *    SilcStream silc_fd_stream_file(const char *filename, SilcBool reading,
 *                                   SilcBool writing, SilcStack stack);
 *
 * DESCRIPTION
 *
 *    Same as silc_fd_stream_create but creates the stream by opening the
 *    file indicated by `filename'.  If the `reading' is TRUE the file is
 *    opened for reading.  If the `writing' is TRUE the file is opened
 *    for writing.  Returns NULL if system is out of memory.
 *
 *    If the silc_stream_set_notifier is called the stream is set to
 *    non-blocking mode.
 *
 *    If `stack' is non-NULL all memory is allocated from the `stack' and
 *    will be released back to `stack' after the stream is destroyed.
 *
 ***/
SilcStream silc_fd_stream_file(const char *filename, SilcBool reading,
			       SilcBool writing, SilcStack stack);

/****f* silcutil/silc_fd_stream_file2
 *
 * SYNOPSIS
 *
 *    SilcStream silc_fd_stream_file2(const char *read_file,
 *                                    const char *write_file,
 *                                    SilcStack stack);
 *
 * DESCRIPTION
 *
 *    Same as silc_fd_stream_file but creates the stream by opening `read_file'
 *    for reading and `write_file' for writing.  Returns NULL if system is
 *    out of memory.
 *
 *    If the silc_stream_set_notifier is called the stream is set to
 *    non-blocking mode.
 *
 *    If `stack' is non-NULL all memory is allocated from the `stack' and
 *    will be released back to `stack' after the stream is destroyed.
 *
 ***/
SilcStream silc_fd_stream_file2(const char *read_file, const char *write_file,
				SilcStack stack);

/****f* silcutil/silc_fd_stream_get_info
 *
 * SYNOPSIS
 *
 *    SilcBool
 *    silc_fd_stream_get_info(SilcStream stream, int *read_fd, int *write_fd);
 *
 * DESCRIPTION
 *
 *    Returns the file descriptors associated with the stream.  The 'write_fd'
 *    is available only if the stream was created with silc_fd_stream_create2
 *    function.  Returns FALSE if the information is not available.
 *
 ***/
SilcBool silc_fd_stream_get_info(SilcStream stream,
				 int *read_fd, int *write_fd);

/* Backwards support */
#define silc_fd_stream_get_error(stream) silc_errno

#endif /* SILCFDSTREAM_H */
