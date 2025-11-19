CC = gcc
CFLAGS = -Wall

TARGET = lab1
SRC = $(TARGET).c

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

.PHONY: all clean