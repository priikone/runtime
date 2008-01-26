#
#  silc.m4
#
#  Author: Pekka Riikonen <priikone@silcnet.org>
#
#  Copyright (C) 2007 Pekka Riikonen
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; version 2 of the License.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#

# Function to check if system has SMP kernel.
#
# Usage: SILC_SYSTEM_IS_SMP([ACTION-IF-FOUND] [, ACTION-IF-NOT-FOUND]
#                           [, ACTION-IF-NOT-DETECTED])
#
# The ACTION-IF-NOT-DETECTED is called if we could not detect whether or 
# not the system is SMP.
#
# x_is_smp variable is set to true or false as a result for calling this
# function.  Caller may use the variable to check for the result in the 
# code.
#
AC_DEFUN([SILC_SYSTEM_IS_SMP],
[
  AC_MSG_CHECKING(whether system has SMP kernel)
  x_is_smp=false

  case "$target" in
    *-*-linux*)
      # Take data from Linux /proc
      if test -f /proc/stat; then
        cpucount=`grep "^cpu" /proc/stat -c 2> /dev/null`
        if test $cpucount -gt 1; then
          AC_DEFINE([SILC_SMP], [], [SILC_SMP])
          AC_MSG_RESULT(yes)
          x_is_smp=true
          ifelse([$1], , :, [$1])
        else
          AC_MSG_RESULT(no)
          ifelse([$2], , :, [$2])
        fi
      else
        AC_MSG_RESULT(no)
        ifelse([$2], , :, [$2])
      fi
      ;;

    *-*-*bsd*)
      # BSDs can have SMP info in sysctl 'kern.smp.cpus' variable
      sysctl="sysctl -n kern.smp.cpus"
      cpucount=`(/sbin/$sysctl 2> /dev/null || \
                 /usr/sbin/$sysctl 2> /dev/null || echo -n 0)`
      if test $cpucount -gt 1; then
        AC_DEFINE([SILC_SMP], [], [SILC_SMP])
        AC_MSG_RESULT(yes)
         x_is_smp=true
        ifelse([$1], , :, [$1])
      else
        AC_MSG_RESULT(no)
        ifelse([$2], , :, [$2])
      fi
      ;;

    *)
      AC_MSG_RESULT(cannot detect on this system)
      ifelse([$3], , :, [$3])
      ;;
  esac
])

# Function to check for CPU feature flags.
#
# Usage: SILC_CPU_FLAG(flag [, ACTION-IF-FOUND] [, ACTION-IF-NOT-FOUND])
#
# x_have_cpu_<flag> variable is set to true or false value as a result for
# calling this function for the <flag>.  Caller may use the variable to
# check the result in the code.
#
AC_DEFUN([SILC_CPU_FLAG],
[
  AC_MSG_CHECKING(whether CPU supports $1)
  x_have_cpu_$1=false

  case "$target" in
    *-*-linux*)
      # Take data from Linux /proc
      if test -f /proc/cpuinfo; then
        cpuflags=`grep "^flags.*$1 " /proc/cpuinfo 2> /dev/null`
        if test $? != 0; then
          AC_MSG_RESULT(no)
          ifelse([$3], , :, [$3])
        else
          AC_MSG_RESULT(yes)
          x_have_cpu_$1=true
          ifelse([$2], , :, [$2])
        fi
      else
        AC_MSG_RESULT(no)
        ifelse([$3], , :, [$3])
      fi
      ;;

    *-*-*bsd*)
      # BSDs have some flags in sysctl 'machdep' variable
      cpuflags=`/sbin/sysctl machdep 2> /dev/null | grep "\.$1.*.1"`
      if test $? != 0; then
        AC_MSG_RESULT(no)
        ifelse([$3], , :, [$3])
      else
        AC_MSG_RESULT(yes)
        x_have_cpu_$1=true
        ifelse([$2], , :, [$2])
      fi
      ;;

    *)
      AC_MSG_RESULT(no, cannot detect on this system)
      ifelse([$3], , :, [$3])
      ;;
  esac
])

# Function to check if compiler option works with the compiler.  If you
# want the option added to some other than CFLAGS variable use the
# SILC_ADD_CC_FLAGS which supports to specifiable destination variable.
#
# Usage: SILC_ADD_CFLAGS(FLAGS, [ACTION-IF-FAILED])
#
AC_DEFUN([SILC_ADD_CFLAGS],
[ tmp_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS $1"
  AC_MSG_CHECKING(whether $CC accepts $1 flag)
  AC_TRY_LINK([], [], [AC_MSG_RESULT(yes)], [AC_MSG_RESULT(no)
				       CFLAGS="$tmp_CFLAGS"
				       $2])
  unset tmp_CFLAGS
])

# Function to check if compiler option works with the compiler,
# destination variable specifiable
#
# Usage: SILC_ADD_CC_FLAGS(VAR, FLAGS, [ACTION-IF-FAILED])
#
AC_DEFUN([SILC_ADD_CC_FLAGS],
[ tmp_CFLAGS="$1_CFLAGS"
  $1_CFLAGS="${$1_CFLAGS} $2"
  AC_MSG_CHECKING(whether $CC accepts $2 flag)
  AC_TRY_LINK([], [], [AC_MSG_RESULT(yes)], [AC_MSG_RESULT(no)
				       $1_CFLAGS="$tmp_CFLAGS"
				       $3])
  unset tmp_CFLAGS
])
