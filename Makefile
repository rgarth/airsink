CC = gcc
CFLAGS = -Wall -Wextra -I./include
LDFLAGS = -levent -lavahi-client -lavahi-common -lssl -lcrypto

SRCS = src/main.c src/rtsp_server.c src/mdns_avahi.c
OBJS = $(SRCS:.c=.o)
TARGET = airsink

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) 