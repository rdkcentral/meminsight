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
	./$(TARGET) --help

clean:
	@echo "Cleaning up build artifacts..."
	rm -f $(TARGET)
