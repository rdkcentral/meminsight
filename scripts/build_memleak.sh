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

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

printf "%s\n" "${BLUE}================================================${NC}"
printf "%s\n" "${BLUE}  MemLeakUtil Build Script${NC}"
printf "%s\n\n" "${BLUE}================================================${NC}"

# Validate environment
if ! command -v git >/dev/null 2>&1; then
    printf "%s\n" "${RED}ERROR: git not found. Please install git.${NC}"
    exit 1
fi

if ! command -v autoconf >/dev/null 2>&1; then
    printf "%s\n" "${YELLOW}WARNING: autoconf not found. Installing build dependencies...${NC}"
    apt-get update && apt-get install -y autotools-dev autoconf automake pkg-config libtool m4
fi

printf "%s\n" "${BLUE}Configuration:${NC}"
echo "  Work Directory:    ${MEMLEAK_WORK_DIR}"
echo "  Git Branch:        ${MEMLEAK_GIT_BRANCH}"
echo "  Install Prefix:    ${MEMLEAK_INSTALL_DIR}"
echo "  Build Jobs:        ${MEMLEAK_JOBS}"
echo "  GitHub URL:        ${MEMLEAK_GIT_URL}"
printf "\n"

# Step 1: Clone repository
printf "%s\n" "${BLUE}[1/4] Cloning memleakutil repository...${NC}"
MEMLEAK_SRC_DIR="${MEMLEAK_WORK_DIR}/memleakutil"

if [ -d "${MEMLEAK_SRC_DIR}" ]; then
    printf "%s\n" "${YELLOW}  - Updating existing clone${NC}"
    cd "${MEMLEAK_SRC_DIR}"
    git fetch origin
    git checkout "${MEMLEAK_GIT_BRANCH}"
    git pull origin "${MEMLEAK_GIT_BRANCH}"
else
    printf "%s\n" "${YELLOW}  - Fresh clone${NC}"
    cd "${MEMLEAK_WORK_DIR}"
    git clone --depth 1 --branch "${MEMLEAK_GIT_BRANCH}" "${MEMLEAK_GIT_URL}" memleakutil
    cd "${MEMLEAK_SRC_DIR}"
fi

printf "%s\n" "${GREEN}  + Repository ready${NC}"
MEMLEAK_COMMIT=$(git rev-parse --short HEAD)
echo "    Commit: ${MEMLEAK_COMMIT}"
printf "\n"

# Step 2: Configure build
printf "%s\n" "${BLUE}[2/4] Configuring memleakutil build...${NC}"
if [ ! -f configure ]; then
    printf "%s\n" "${YELLOW}  - Running autoreconf${NC}"
    autoreconf -i
fi

CONFIGURE_OPTS="--prefix=${MEMLEAK_INSTALL_DIR} --enable-shared --disable-static"
printf "%s\n" "${YELLOW}  - Configure options: ${CONFIGURE_OPTS}${NC}"
./configure ${CONFIGURE_OPTS}
printf "%s\n\n" "${GREEN}  + Configuration complete${NC}"

# Step 3: Build
printf "%s\n" "${BLUE}[3/4] Building memleakutil library...${NC}"
make clean
make -j"${MEMLEAK_JOBS}"
printf "%s\n\n" "${GREEN}  + Build complete${NC}"

# Step 4: Install
printf "%s\n" "${BLUE}[4/4] Installing memleakutil to ${MEMLEAK_INSTALL_DIR}...${NC}"
make install

# Verify installation
if [ -f "${MEMLEAK_INSTALL_DIR}/lib/libmemfnswrap.so" ]; then
    printf "%s\n\n" "${GREEN}  + Installation successful${NC}"
    printf "%s\n" "${BLUE}Installation Summary:${NC}"
    ls -lh "${MEMLEAK_INSTALL_DIR}/lib/libmemfnswrap.so"
    printf "\n"
    printf "%s\n" "${GREEN}================================================${NC}"
    printf "%s\n" "${GREEN}  memleakutil build SUCCESSFUL${NC}"
    printf "%s\n\n" "${GREEN}================================================${NC}"
    printf "%s\n" "${BLUE}To use in leak detection:${NC}"
    echo "  export LD_PRELOAD=${MEMLEAK_INSTALL_DIR}/lib/libmemfnswrap.so"
    echo "  export LD_LIBRARY_PATH=${MEMLEAK_INSTALL_DIR}/lib:\$LD_LIBRARY_PATH"
    printf "\n"
    printf "%s\n\n" "${BLUE}Then run meminsight or any application to detect leaks.${NC}"
    
    # Save environment for detect_leak.sh
    echo "export MEMLEAK_INSTALL_DIR=${MEMLEAK_INSTALL_DIR}" > "${MEMLEAK_WORK_DIR}/.memleak_env"
    echo "export MEMLEAK_GIT_COMMIT=${MEMLEAK_COMMIT}" >> "${MEMLEAK_WORK_DIR}/.memleak_env"
    echo "export MEMLEAK_BUILD_DATE=$(date -u)" >> "${MEMLEAK_WORK_DIR}/.memleak_env"
    
    exit 0
else
    printf "%s\n" "${RED}  x Installation FAILED${NC}"
    echo "    libmemfnswrap.so not found at ${MEMLEAK_INSTALL_DIR}/lib/"
    exit 1
fi
