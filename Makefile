PREFIX = /usr/local

CDEBUGFLAGS = -Os -g -Wall

DEFINES = $(PLATFORM_DEFINES)

CFLAGS = $(CDEBUGFLAGS) $(DEFINES) $(EXTRA_DEFINES)

SRCS = dht11.c errorcheck.c config.c

OBJS = dht11.o errorcheck.o config.o

LDLIBS = -lrt -lwiringPi

dht11: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o dht11 $(OBJS) $(LDLIBS)

.PHONY: all install.minimal install

all: dht11

install.minimal: all

install: all install.minimal

.PHONY: uninstall

uninstall:

.PHONY: clean

clean:
	-rm -f dht11
	-rm -f *.o *~ core TAGS gmon.out
