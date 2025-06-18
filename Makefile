CC = gcc
CFLAGS = -Wall -Wextra -I./include
LDFLAGS = -levent -lavahi-client -lavahi-common -lssl -lcrypto -ljson-c

SRCS = src/main.c src/rtsp/rtsp_server.c src/mdns/mdns_avahi.c src/auth/auth.c
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