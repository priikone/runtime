/*

  silcthread_i.h

  Author: Pekka Riikonen <priikone@silcnet.org>

  Copyright (C) 2007 - 2008 Pekka Riikonen

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

*/

#ifndef SILCTHREAD_I_H
#define SILCTHREAD_I_H

#ifndef SILCTHREAD_H
#error "Do not include this header directly"
#endif

/* Thread-local storage structure.  This structure is saved to each thread's
   Tls if the SILC Tls API is used.  This structure must be allocatable
   with silc_calloc and freeable with silc_free, and must also be able to
   pre-allocate from stack. */
typedef struct SilcTlsObject {
  SilcMutex lock;			    /* Global lock, shared */
  SilcHashTable variables;		    /* Global variables, shared */
  SilcHashTable tls_variables;	            /* Tls variables */
  SilcStack stack;			    /* Thread's stack */
  SilcSchedule schedule;		    /* Thread's scheduler */
  void *thread_context;		            /* Context set with SILC Tls API */
  void *platform_context;	            /* Platform specific context */
  char error_reason[256];		    /* Reason for the error */
  SilcResult error;			    /* Errno, last error */
  unsigned int shared_data     : 1;	    /* Set when shares data with other
					       threads in the Tls. */
} *SilcTls, SilcTlsStruct;

/* The internal Tls API.  Implementation is platform specific. */

/* Initializes Tls for current thread.  Must be called for each thread to
   allocate Tls for the thread, including the main thread. */
SilcTls silc_thread_tls_init(void);

/* Return current thread's Tls structure. */
SilcTls silc_thread_get_tls(void);

/* Uninitialize whole Tls system (free shared data), called only once per
   process. */
void silc_thread_tls_uninit(void);

#endif /* SILCTHREAD_I_H */
