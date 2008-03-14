/*

  silcutil.h

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

/****h* silcutil/Misc Utilities
 *
 * DESCRIPTION
 *
 *    Utility functions.
 *
 ***/

#ifndef SILCUTIL_H
#define SILCUTIL_H

/****f* silcutil/silc_gets
 *
 * SYNOPSIS
 *
 *    int silc_gets(char *dest, int destlen, const char *src, int srclen,
 *                  int begin);
 *
 * DESCRIPTION
 *
 *    Gets line from a buffer. Stops reading when a newline or EOF occurs.
 *    This doesn't remove the newline sign from the destination buffer. The
 *    argument begin is returned and should be passed again for the function.
 *
 ***/
int silc_gets(char *dest, int destlen, const char *src, int srclen, int begin);

/****f* silcutil/silc_to_upper
 *
 * SYNOPSIS
 *
 *    SilcBool silc_to_upper(const char *string, char *dest,
 *                           SilcUInt32 dest_size);
 *
 * DESCRIPTION
 *
 *    Converts string to capital characters.
 *
 ***/
SilcBool silc_to_upper(const char *string, char *dest, SilcUInt32 dest_size);

/****f* silcutil/silc_to_lower
 *
 * SYNOPSIS
 *
 *    SilcBool silc_to_lower(const char *string, char *dest,
 *                           SilcUInt32 dest_size);
 *
 * DESCRIPTION
 *
 *    Converts string to capital characters.
 *
 ***/
SilcBool silc_to_lower(const char *string, char *dest, SilcUInt32 dest_size);

/****f* silcutil/silc_parse_userfqdn
 *
 * SYNOPSIS
 *
 *    int silc_parse_userfqdn(const char *string,
 *                            char *user, SilcUInt32 user_size,
 *                            char *fqdn, SilcUInt32 fqdn_size);
 *
 * DESCRIPTION
 *
 *    Parse userfqdn string which is in user@fqdn format.  Returns 0 on
 *    error, 1 if `user' was filled and 2 if both `user' and `fqdn'
 *    was filled.
 *
 ***/
int silc_parse_userfqdn(const char *string,
			char *user, SilcUInt32 user_size,
			char *fqdn, SilcUInt32 fqdn_size);

/****f* silcutil/silc_parse_command_line
 *
 * SYNOPSIS
 *
 *    void silc_parse_command_line(unsigned char *buffer,
 *                                 unsigned char ***parsed,
 *                                 SilcUInt32 **parsed_lens,
 *                                 SilcUInt32 **parsed_types,
 *                                 SilcUInt32 *parsed_num,
 *                                 SilcUInt32 max_args);
 *
 * DESCRIPTION
 *
 *    Parses command line. At most `max_args' is taken. Rest of the line
 *    will be allocated as the last argument if there are more than `max_args'
 *    arguments in the line. Note that the command name is counted as one
 *    argument and is saved.
 *
 ***/
void silc_parse_command_line(unsigned char *buffer,
			     unsigned char ***parsed,
			     SilcUInt32 **parsed_lens,
			     SilcUInt32 **parsed_types,
			     SilcUInt32 *parsed_num,
			     SilcUInt32 max_args);

/****f* silcutil/silc_format
 *
 * SYNOPSIS
 *
 *    char *silc_format(char *fmt, ...);
 *
 * DESCRIPTION
 *
 *    Formats arguments to a string and returns it after allocating memory
 *    for it. It must be remembered to free it later.
 *
 ***/
char *silc_format(char *fmt, ...);

/****f* silcutil/silc_fingerprint
 *
 * SYNOPSIS
 *
 *    char *silc_fingerprint(const unsigned char *data, SilcUInt32 data_len);
 *
 * DESCRIPTION
 *
 *    Return a textual representation of the fingerprint in *data, the
 *    caller must free the returned string.  Returns NULL on error.  If
 *    the `data_len' is longer than 255 bytes, only the first 255 bytes are
 *    used to create the fingerprint.
 *
 ***/
char *silc_fingerprint(const unsigned char *data, SilcUInt32 data_len);

/****f* silcutil/silc_string_is_ascii
 *
 * SYNOPSIS
 *
 *    SilcBool silc_string_is_ascii(const unsigned char *data,
 *                              SilcUInt32 data_len);
 *
 * DESCRIPTION
 *
 *    Return TRUE if the `data' is ASCII string.
 *
 ***/
SilcBool silc_string_is_ascii(const unsigned char *data, SilcUInt32 data_len);

/****f* silcutil/silc_get_input
 *
 * SYNOPSIS
 *
 *    char *silc_get_input(const char *prompt, SilcBool echo_off);
 *
 * DESCRIPTION
 *
 *    Displays input prompt on command line and takes input data from user.
 *
 ***/
char *silc_get_input(const char *prompt, SilcBool echo_off);

/* System dependant prototypes */

/****f* silcutil/silc_get_username
 *
 * SYNOPSIS
 *
 *    char *silc_get_username();
 *
 * DESCRIPTION
 *
 *    Returns the username of the user. If the global variable LOGNAME
 *    does not exists we will get the name from the passwd file.  The
 *    caller must free the returned name.
 *
 *    This function is system dependant.
 *
 ***/
char *silc_get_username();

/****f* silcutil/silc_get_real_name
 *
 * SYNOPSIS
 *
 *    char *silc_get_real_name();
 *
 * DESCRIPTION
 *
 *    Returns the real name of ther user from the passwd file.  The
 *    caller must free the returned name.
 *
 *    This function is system dependant.
 *
 ***/
char *silc_get_real_name();

/****f* silcutil/silc_va_copy
 *
 * SYNOPSIS
 *
 *    void silc_va_copy(va_list dest, va_list src);
 *
 * DESCRIPTION
 *
 *    Copies variable argument list.  This must be called in case the
 *    variable argument list must be evaluated multiple times.  For each
 *    evaluation the list must be copied and va_end must be called for
 *    each copied list.
 *
 ***/
#define silc_va_copy(dest, src) __SILC_VA_COPY(dest, src)

/****f* silcutil/silc_hexdump
 *
 * SYNOPSIS
 *
 *    void silc_hexdump(const unsigned char *data, SilcUInt32 data_len,
 *                      FILE *output);
 *
 * DESCRIPTION
 *
 *    Dumps the `data' of length of `data_len' bytes as HEX.  The `output'
 *    file specifies the destination.
 *
 ***/
void silc_hexdump(const unsigned char *data, SilcUInt32 data_len,
		  FILE *output);

/****f* silcutil/silc_hex2data
 *
 * SYNOPSIS
 *
 *    SilcBool silc_hex2data(const char *hex, unsigned char *data,
 *                           SilcUInt32 data_size, SilcUInt32 *ret_data_len);
 *
 * DESCRIPTION
 *
 *    Converts HEX character string to binary data.  Each HEX numbers must
 *    have two characters in the `hex' string.
 *
 ***/
SilcBool silc_hex2data(const char *hex, unsigned char *data,
		       SilcUInt32 data_size, SilcUInt32 *ret_data_len);

/****f* silcutil/silc_data2hex
 *
 * SYNOPSIS
 *
 *    SilcBool silc_data2hex(const unsigned char *data, SilcUInt32 data_len,
 *                           char *hex, SilcUInt32 hex_size);
 *
 * DESCRIPTION
 *
 *    Converts binary data to HEX string.  This NULL terminates the `hex'
 *    buffer automatically.
 *
 ***/
SilcBool silc_data2hex(const unsigned char *data, SilcUInt32 data_len,
		       char *hex, SilcUInt32 hex_size);

/****f* silcutil/silc_get_num_cpu
 *
 * SYNOPSIS
 *
 *    int silc_get_num_cpu(void);
 *
 * DESCRIPTION
 *
 *    Returns the number of CPU cores in the current machine.  Returns 0
 *    if the routine could not determine the number of cores.
 *
 ***/
int silc_get_num_cpu(void);

#endif	/* !SILCUTIL_H */
