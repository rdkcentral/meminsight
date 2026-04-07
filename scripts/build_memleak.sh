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

#
# build_memleak.sh - Clone and build memleakutil library for memory leak detection
# 
# Purpose: Provides isolated, repeatable build of memleakutil (libmemfnswrap.so)
#          Used by CI/CD pipelines and developer workflows for memory profiling
#
# Usage:
#   sh scripts/build_memleak.sh [OUTPUT_DIR] [GIT_BRANCH]
#
# Environment Variables:
#   MEMLEAK_INSTALL_DIR  - Installation prefix (default: /tmp/memleakutil-install)
#   MEMLEAK_JOBS         - Parallel build jobs (default: 4)
#   MEMLEAK_GIT_URL      - Git repository URL (default: GitHub)

set -e

# Script directory
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "${SCRIPT_DIR}")"

# Configuration
MEMLEAK_WORK_DIR="${1:-/tmp}"
MEMLEAK_GIT_BRANCH="${2:-main}"
MEMLEAK_INSTALL_DIR="${MEMLEAK_INSTALL_DIR:-/tmp/memleakutil-install}"
MEMLEAK_JOBS="${MEMLEAK_JOBS:-4}"
MEMLEAK_GIT_URL="${MEMLEAK_GIT_URL:-https://github.com/JagadheesanD/memleakutil.git}"

printf "%s\n" "================================================"
printf "%s\n" "  MemLeakUtil Build Script"
printf "%s\n\n" "================================================"

# Validate environment
if ! command -v git >/dev/null 2>&1; then
    printf "%s\n" "ERROR: git not found. Please install git."
    exit 1
fi

if ! command -v autoconf >/dev/null 2>&1; then
    printf "%s\n" "WARNING: autoconf not found. Installing build dependencies..."
    apt-get update && apt-get install -y autotools-dev autoconf automake pkg-config libtool m4
fi

printf "%s\n" "Configuration:"
echo "  Work Directory:    ${MEMLEAK_WORK_DIR}"
echo "  Git Branch:        ${MEMLEAK_GIT_BRANCH}"
echo "  Install Prefix:    ${MEMLEAK_INSTALL_DIR}"
echo "  Build Jobs:        ${MEMLEAK_JOBS}"
echo "  GitHub URL:        ${MEMLEAK_GIT_URL}"
printf "\n"

# Step 1: Clone repository
printf "%s\n" "[1/5] Cloning memleakutil repository..."
MEMLEAK_SRC_DIR="${MEMLEAK_WORK_DIR}/memleakutil"

if [ -d "${MEMLEAK_SRC_DIR}" ]; then
    printf "%s\n" "  - Updating existing clone"
    cd "${MEMLEAK_SRC_DIR}"
    git fetch origin
    git checkout "${MEMLEAK_GIT_BRANCH}"
    git pull origin "${MEMLEAK_GIT_BRANCH}"
else
    printf "%s\n" "  - Fresh clone"
    cd "${MEMLEAK_WORK_DIR}"
    git clone --depth 1 --branch "${MEMLEAK_GIT_BRANCH}" "${MEMLEAK_GIT_URL}" memleakutil
    cd "${MEMLEAK_SRC_DIR}"
fi

printf "%s\n" "  + Repository ready"
MEMLEAK_COMMIT=$(git rev-parse --short HEAD)
echo "    Commit: ${MEMLEAK_COMMIT}"
printf "\n"

# Step 2: Cleanup previous artifacts
printf "%s\n" "[2/5] Cleaning previous build artifacts..."
if [ -f Makefile ]; then
    make distclean >/dev/null 2>&1 || make clean >/dev/null 2>&1 || true
fi
rm -rf "${MEMLEAK_INSTALL_DIR}"
printf "%s\n\n" "  + Cleanup complete"

# Step 3: Configure build
printf "%s\n" "[3/5] Configuring memleakutil build..."
printf "%s\n" "  - Regenerating autotools files"

BUILD_MODE="autotools"
mkdir -p m4
if command -v libtoolize >/dev/null 2>&1; then
    libtoolize --force --copy >/dev/null 2>&1 || true
fi

if ! autoreconf -fiv; then
    printf "%s\n" "  - Autotools regeneration failed; falling back to Makefile.raw"
    BUILD_MODE="raw"
fi

if [ "${BUILD_MODE}" = "raw" ] && [ ! -f Makefile.raw ]; then
    printf "%s\n" "ERROR: Makefile.raw not found; cannot continue fallback build"
    exit 1
fi

if [ "${BUILD_MODE}" = "autotools" ]; then
    CONFIGURE_OPTS="--prefix=${MEMLEAK_INSTALL_DIR}"
    printf "%s\n" "  - Configure options: ${CONFIGURE_OPTS}"
    if ! ./configure ${CONFIGURE_OPTS}; then
        printf "%s\n" "  - configure failed; falling back to Makefile.raw"
        BUILD_MODE="raw"
    fi
fi

printf "%s\n\n" "  + Configuration complete (mode: ${BUILD_MODE})"

# Step 4: Build
printf "%s\n" "[4/5] Building memleakutil library..."
if [ "${BUILD_MODE}" = "autotools" ]; then
    make clean
    make -j"${MEMLEAK_JOBS}"
else
    make -f Makefile.raw clean || true
    make -f Makefile.raw -j"${MEMLEAK_JOBS}"
fi
printf "%s\n\n" "  + Build complete"

# Step 5: Install
printf "%s\n" "[5/5] Installing memleakutil to ${MEMLEAK_INSTALL_DIR}..."
if [ "${BUILD_MODE}" = "autotools" ]; then
    make install
else
    mkdir -p "${MEMLEAK_INSTALL_DIR}/lib" "${MEMLEAK_INSTALL_DIR}/bin"
    cp -af libmemfnswrap.so* "${MEMLEAK_INSTALL_DIR}/lib/"
    if [ -f memleakutil ]; then
        cp -f memleakutil "${MEMLEAK_INSTALL_DIR}/bin/"
    fi
fi

# Verify installation
if [ -f "${MEMLEAK_INSTALL_DIR}/lib/libmemfnswrap.so" ]; then
    printf "%s\n\n" "  + Installation successful"
    printf "%s\n" "Installation Summary:"
    ls -lh "${MEMLEAK_INSTALL_DIR}/lib/libmemfnswrap.so"
    printf "\n"
    printf "%s\n" "================================================"
    printf "%s\n" "  memleakutil build SUCCESSFUL"
    printf "%s\n\n" "================================================"
    printf "%s\n" "To use in leak detection:"
    echo "  export LD_PRELOAD=${MEMLEAK_INSTALL_DIR}/lib/libmemfnswrap.so"
    echo "  export LD_LIBRARY_PATH=${MEMLEAK_INSTALL_DIR}/lib:\$LD_LIBRARY_PATH"
    printf "\n"
    printf "%s\n\n" "Then run meminsight or any application to detect leaks."
    
    # Save environment for detect_leak.sh
    echo "export MEMLEAK_INSTALL_DIR=${MEMLEAK_INSTALL_DIR}" > "${MEMLEAK_WORK_DIR}/.memleak_env"
    echo "export MEMLEAK_GIT_COMMIT=${MEMLEAK_COMMIT}" >> "${MEMLEAK_WORK_DIR}/.memleak_env"
    echo "export MEMLEAK_BUILD_DATE=$(date -u)" >> "${MEMLEAK_WORK_DIR}/.memleak_env"
    
    exit 0
else
    printf "%s\n" "  x Installation FAILED"
    echo "    libmemfnswrap.so not found at ${MEMLEAK_INSTALL_DIR}/lib/"
    exit 1
fi
