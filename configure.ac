##########################################################################
# Copyright 2024 Comcast Cable Communications Management, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
##########################################################################
#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([meminsight], [1.0])
AM_INIT_AUTOMAKE([foreign subdir-objects])

# Config Source files.
AC_CONFIG_SRCDIR([src/meminsight.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL

# Checks for header files.
AC_CHECK_HEADERS([stdio.h string.h unistd.h stdlib.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# ---------------------------------------------------------------------------
# Optional feature: cJSON JSON output support (runtime dlopen, no hard link)
# ---------------------------------------------------------------------------
AC_ARG_ENABLE([cjson],
  [AS_HELP_STRING([--enable-cjson],
    [Enable JSON output support (cJSON loaded at runtime via dlopen) @<:@default=no@:>@])],
  [case "${enableval}" in
    yes) enable_cjson=yes ;;
    no)  enable_cjson=no  ;;
    *)   AC_MSG_ERROR([bad value ${enableval} for --enable-cjson]) ;;
   esac],
  [enable_cjson=no])

AS_IF([test "x$enable_cjson" = "xyes"], [
  AC_DEFINE([ENABLE_CJSON], [1], [Define to enable JSON output via dlopen cJSON])
  # We use dlopen at runtime, so we only need libdl at link time
  AC_CHECK_LIB([dl], [dlopen], [],
    [AC_MSG_ERROR([libdl not found; required for runtime cJSON loading])])
])

# ---------------------------------------------------------------------------
# Optional feature: test mode (TESTME flag)
# ---------------------------------------------------------------------------
AC_ARG_ENABLE([test],
  [AS_HELP_STRING([--enable-test],
    [Enable test mode with TESTME flag @<:@default=no@:>@])],
  [case "${enableval}" in
    yes) enable_test=yes ;;
    no)  enable_test=no  ;;
    *)   AC_MSG_ERROR([bad value ${enableval} for --enable-test]) ;;
   esac],
  [enable_test=no])

AS_IF([test "x$enable_test" = "xyes"], [
  AC_DEFINE([TESTME], [1], [Define to enable test mode])
])

AM_CONDITIONAL([ENABLE_CJSON], [test "x$enable_cjson" = "xyes"])
AM_CONDITIONAL([ENABLE_TEST],  [test "x$enable_test"  = "xyes"])

AC_CONFIG_FILES([Makefile])

AC_OUTPUT

