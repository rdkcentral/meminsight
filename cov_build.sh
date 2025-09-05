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
    echo "Clean complete."
    exit 0
fi

# Parse output format parameter
FORMAT="csv"  # Default to CSV
ENABLE_CJSON="no"
if [ "$1" = "json" ]; then
    FORMAT="json"
    echo "Building with default output format: JSON"
elif [ "$1" = "csv" ]; then
    FORMAT="csv"
    echo "Building with default output format: CSV"
elif [ -n "$1" ]; then
    echo "Warning: Unknown parameter '$1'. Valid options are 'json' or 'csv'."
    echo "Defaulting to CSV format."
fi

if [ "$2" = "cjson" ]; then
    ENABLE_CJSON="yes"
    echo "Building with cJSON support enabled."
elif [ -n "$2" ]; then
    echo "Warning: Unknown parameter '$2'. Valid options are 'cjson'."
    echo "Defaulting to no cJSON support."
fi

echo "Running build steps..."

# Check if required tools are installed and install them if needed
echo "Checking for required build tools..."
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
    echo "Missing tools:$MISSING_TOOLS. Attempting to install..."
    if command -v apt-get >/dev/null 2>&1; then
        apt-get update && apt-get install -y $MISSING_TOOLS
    elif command -v yum >/dev/null 2>&1; then
        yum install -y $MISSING_TOOLS
    else
        echo "Please install the following tools manually:$MISSING_TOOLS"
        exit 1
    fi
fi

# Use autoreconf for better compatibility
echo "Running autoreconf..."
autoreconf --install --verbose --force

echo "Running configure..."
CJSON_OPTION=""
if [ "$ENABLE_CJSON" = "yes" ]; then
    CJSON_OPTION="--enable-cjson"
fi
./configure --with-format=$FORMAT $CJSON_OPTION

echo "Running make..."
make

echo "Build complete."
