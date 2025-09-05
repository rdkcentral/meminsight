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

AC_INIT([xmeminsight], [1.0])
AM_INIT_AUTOMAKE([foreign subdir-objects])

# Config Source files.
AC_CONFIG_SRCDIR([src/memstatus.c])
AC_CONFIG_HEADERS([config.h])

# Default output format
AC_ARG_WITH([format],
  [AS_HELP_STRING([--with-format=FORMAT],
    [Default output format (csv or json) @<:@default=csv@:>@])],
  [OUTPUT_FORMAT="$withval"],
  [OUTPUT_FORMAT="csv"])

# cJSON support
AC_ARG_ENABLE([cjson],
  [AS_HELP_STRING([--enable-cjson],
    [Enable cJSON support for JSON output @<:@default=no@:>@])],
  [case "${enableval}" in
    yes) enable_cjson=yes ;;
    no)  enable_cjson=no ;;
    *)   AC_MSG_ERROR([bad value ${enableval} for --enable-cjson]) ;;
   esac],
  [enable_cjson=no])

# Check for cJSON if enabled
AS_IF([test "x$enable_cjson" = "xyes"], [
  AC_CHECK_HEADER([cjson/cJSON.h],
    [AC_DEFINE([ENABLE_CJSON], [1], [Define if cJSON is enabled])
     AC_SEARCH_LIBS([cJSON_Parse], [cjson],
       [],
       [AC_MSG_ERROR([cJSON library not found, install libcjson-dev or similar])])],
    [AC_MSG_ERROR([cJSON headers not found, install libcjson-dev or similar])])
])

# Define conditional for automake
AM_CONDITIONAL([ENABLE_CJSON], [test "x$enable_cjson" = "xyes"])

# Define format in config.h
if test "$OUTPUT_FORMAT" = "json"; then
  if test "x$enable_cjson" != "xyes"; then
    AC_MSG_WARN([JSON format requested but cJSON support not enabled. Building with CSV format as default.])
    AC_DEFINE([DEFAULT_FORMAT_CSV], [1], [Default output format is CSV])
  else
    AC_DEFINE([DEFAULT_FORMAT_JSON], [1], [Default output format is JSON])
  fi
elif test "$OUTPUT_FORMAT" = "csv"; then
  AC_DEFINE([DEFAULT_FORMAT_CSV], [1], [Default output format is CSV])
else
  # Default to CSV for unknown formats
  AC_DEFINE([DEFAULT_FORMAT_CSV], [1], [Default output format is CSV])
fi

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL

# Checks for header files.
AC_CHECK_HEADERS([stdio.h string.h unistd.h stdlib.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

AC_CONFIG_FILES([Makefile])

AC_OUTPUT

