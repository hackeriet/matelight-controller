
OBJS			= game.o

TARGET			= game

CC				= gcc
LD				= gcc

CFLAGS			= -Wall -W -Wextra
LDFLAGS			=
CFLAGS			+= --std=gnu99

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

all: $(TARGET)

$(TARGET): $(OBJS)
	$(LD) -o $@ $^ $(LDFLAGS)

%.o: %.c *.h
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean
