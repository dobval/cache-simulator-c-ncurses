CC = gcc
CFLAGS = -Wall -Wextra -g -std=c11
LDFLAGS = -lncurses

TARGET = cache_sim
SRCS = src/main.c src/cache.c src/ui.c src/trace.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET)
