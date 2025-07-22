#!/bin/sh
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

set -e

if [ "$1" = "--clean" ]; then
    echo "Cleaning build artifacts..."
    make clean || true
    rm -f config.h config.h.in config.log config.status
    rm -rf autom4te.cache
    rm -f Makefile Makefile.in aclocal.m4 configure ltmain.sh libtool
    rm -f install-sh missing depcomp compile
    rm -f *.o *.lo *.la *.al *.so *.a meminsight
    rm -f stamp-h1
    rm -rf .deps/
    rm -rf src/.deps/
    echo "Clean complete."
    exit 0
fi

echo "Running build steps..."
aclocal
autoheader
autoconf
automake --add-missing
./configure
make
echo "Build complete."
