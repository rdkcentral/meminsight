CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = xMemInsight

all: $(TARGET)
	@echo "Build complete: $(TARGET)"

$(TARGET): memstatus.c memstatus.h
	@echo "Compiling memstatus.c to create $(TARGET)..."
	$(CC) $(CFLAGS) -o $(TARGET) memstatus.c

clean:
	@echo "Cleaning up build artifacts..."
	rm -f $(TARGET)
