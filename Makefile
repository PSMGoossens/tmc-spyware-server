
PROGRAM=tmc-spyware-server-cgi
CURL_CFLAGS=$(shell pkg-config --cflags libcurl)
CURL_LIBS=$(shell pkg-config --libs libcurl)

SOURCES=$(PROGRAM).c auth.c datastream.c
HEADERS=config-defaults.h config.h common.h auth.h datastream.h

all: $(PROGRAM)

config.h:
	touch $@

$(PROGRAM): $(SOURCES) $(HEADERS)
	gcc -std=c99 -Wall -Os -D_FILE_OFFSET_BITS=64 $(CURL_CFLAGS) -o $@ $(SOURCES) $(CURL_LIBS)

clean:
	rm -f $(PROGRAM)
