CC = gcc
CFLAGS = -Wall -Wextra -std=c99
TARGET = tarsau

all: $(TARGET)

$(TARGET): tarsau.c
	$(CC) $(CFLAGS) -o $(TARGET) tarsau.c

clean:
	rm -f $(TARGET) *.sau
