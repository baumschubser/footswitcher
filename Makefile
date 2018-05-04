INSTALL = /usr/bin/install -c
INSTALLDATA = /usr/bin/install -c -m 644
PROGNAME = footswitcher
CFLAGS = -Wall
UNAME := $(shell uname)
ifeq ($(UNAME), Darwin)
	CFLAGS += -DOSX $(shell pkg-config --cflags hidapi)
	LDFLAGS = $(shell pkg-config --libs hidapi)
else
	ifeq ($(UNAME), Linux)
		CFLAGS += $(shell pkg-config --cflags hidapi-libusb)
		LDFLAGS = $(shell pkg-config --libs hidapi-libusb)
	else
		LDFLAGS = -lhidapi
	endif
endif

all: $(PROGNAME)

$(PROGNAME): $(PROGNAME).c footswitch/common.h footswitch/common.c footswitch/debug.h footswitch/debug.c
	$(CC) $(PROGNAME).c footswitch/common.c footswitch/debug.c -o $(PROGNAME) $(CFLAGS) $(LDFLAGS)

install: all
	$(INSTALL) $(PROGNAME) /usr/local/bin

clean:
	rm -f $(PROGNAME) *.o

