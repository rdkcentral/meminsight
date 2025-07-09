CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = xMemInsight

all: $(TARGET)

$(TARGET): memstatus.c memstatus.h
	$(CC) $(CFLAGS) -o $(TARGET) memstatus.c

clean:
	rm -f $(TARGET)
