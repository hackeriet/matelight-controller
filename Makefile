
OBJS			= main.o ip.o mdns.o wledapi.o input.o mqtt.o announce.o snake.o tetris.o flappy.o pong.o

TARGET			= matelight

CC				= gcc
LD				= gcc

CFLAGS			= -Wall -W -Wextra
LDFLAGS			= -pthread
CFLAGS			+= --std=gnu99 -pthread

ifdef RELEASE
CFLAGS			+= -O2 -fomit-frame-pointer
CFLAGS			+= -DNDEBUG
LDFLAGS			+= -s
else
CFLAGS			+= -DDEBUG
CFLAGS			+= -O0 -ggdb
endif

CFLAGS			+= -pipe

LDFLAGS			+= -lm

CFLAGS			+= $(shell pkg-config avahi-client --cflags)
LDFLAGS			+= $(shell pkg-config avahi-client --libs)

CFLAGS			+= $(shell pkg-config libcurl --cflags)
LDFLAGS			+= $(shell pkg-config libcurl --libs)

CFLAGS			+= $(shell pkg-config libudev --cflags)
LDFLAGS			+= $(shell pkg-config libudev --libs)

CFLAGS			+= $(shell pkg-config libmosquitto --cflags)
LDFLAGS			+= $(shell pkg-config libmosquitto --libs)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(LD) -o $@ $^ $(LDFLAGS)

%.o: %.c *.h
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f $(TARGET) $(OBJS)

install:
	install -m 755 $(TARGET) /usr/local/bin/

.PHONY: all clean install
