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
#

CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = xMemInsight

all: $(TARGET)
	@echo "Build complete: $(TARGET)"

$(TARGET): memstatus.c memstatus.h
	@echo "Compiling memstatus.c to create $(TARGET)..."
	$(CC) $(CFLAGS) -o $(TARGET) memstatus.c

run: $(TARGET)
	@echo "Running standalone $(TARGET)..."
	./$(TARGET)

test: $(TARGET)
	@echo "Running minimal tests for $(TARGET)..."
	./$(TARGET) --test

help: $(TARGET)
	@echo "Showing help for $(TARGET)..."
	-./$(TARGET) --help

clean:
	@echo "Cleaning up build artifacts..."
	rm -f $(TARGET)
