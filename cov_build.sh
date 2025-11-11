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

ENABLE_TESTME=0
CLEAN_ONLY=0

while [ $# -gt 0 ]; do
    case "$1" in
        --clean)
            CLEAN_ONLY=1
            ;;
        --enable-testme)
            ENABLE_TESTME=1
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  --clean          Clean build artifacts and exit"
            echo "  --enable-testme  Enable TESTME compilation flag for test mode"
            echo "  --help, -h       Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
    shift
done

if [ "$CLEAN_ONLY" = "1" ]; then
    echo "[cov_build] Cleaning build artifacts..."
    make clean 2>/dev/null || true
    make distclean 2>/dev/null || true
    rm -f config.h config.h.in config.log config.status
    rm -rf autom4te.cache
    rm -f Makefile Makefile.in aclocal.m4 configure ltmain.sh libtool
    rm -f install-sh missing depcomp compile config.guess config.sub
    rm -f *.o *.lo *.la *.al *.so *.a xmeminsight
    rm -f stamp-h1
    rm -rf .deps/
    rm -f configure~
    rm -rf config.h.in~
    rm -rf src/.deps/
    echo "[cov_build] Clean complete."
    exit 0
fi

echo "[cov_build] Running build steps..."

# Check if required tools are installed and install them if needed
echo "[cov_build] Checking for required build tools..."
MISSING_TOOLS=""

if ! command -v autoconf >/dev/null 2>&1; then
    MISSING_TOOLS="$MISSING_TOOLS autoconf"
fi

if ! command -v automake >/dev/null 2>&1; then
    MISSING_TOOLS="$MISSING_TOOLS automake"
fi

if ! command -v autoreconf >/dev/null 2>&1; then
    MISSING_TOOLS="$MISSING_TOOLS autotools-dev"
fi

if [ -n "$MISSING_TOOLS" ]; then
    echo "[cov_build] Missing tools:$MISSING_TOOLS. Attempting to install..."
    if command -v apt-get >/dev/null 2>&1; then
        apt-get update && apt-get install -y $MISSING_TOOLS
    elif command -v yum >/dev/null 2>&1; then
        yum install -y $MISSING_TOOLS
    else
        echo "[cov_build] Please install the following tools manually:$MISSING_TOOLS"
        exit 1
    fi
fi

[ "$ENABLE_TESTME" = "1" ] && echo "[cov_build] Compiling with -DTESTME flag"

echo "[cov_build] Running autoreconf..."
autoreconf --install --verbose --force

echo "[cov_build] Running configure..."
./configure

echo "[cov_build] Running make..."
if [ "$ENABLE_TESTME" = "1" ]; then
    echo "[cov_build] TESTME mode enabled. Passing compilation flag..."
    make CPPFLAGS="-DTESTME"
else
    make
fi

echo "[cov_build] Build complete."
