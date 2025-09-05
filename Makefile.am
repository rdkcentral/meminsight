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

# Set automake options
AUTOMAKE_OPTIONS = subdir-objects

bin_PROGRAMS = xmeminsight

xmeminsight_SOURCES = src/memstatus.c
xmeminsightdir = $(includedir)
xmeminsight_HEADERS = src/memstatus.h

AM_CPPFLAGS = @CPPFLAGS@

# Linke against cJSON if enabled
if ENABLE_CJSON
xmeminsight_LDADD = -lcjson
endif

# Add clean targets for better cleanup
CLEANFILES = *.o *.lo *.la *.al *.so *.a
DISTCLEANFILES = config.h config.h.in config.log config.status Makefile.in aclocal.m4 configure

