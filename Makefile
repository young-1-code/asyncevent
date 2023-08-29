CC := gcc
TARGET := mainApp
CFLGAS = -g
LDFLAGS := -lpthread

SRCS := $(wildcard *.c)

all:
	$(CC) $(SRCS) -o $(TARGET) $(CFLGAS) $(LDFLAGS)

.PHONY:clean
clean:
	@rm $(TARGET)
