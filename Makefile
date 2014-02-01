
BINARY=cgi-bin/tmc-spyware-server-cgi
CURL_CFLAGS=$(shell pkg-config --cflags libcurl)
CURL_LIBS=$(shell pkg-config --libs libcurl)

DEP_CFLAGS=$(CURL_CFLAGS)
DEP_LIBS=$(CURL_LIBS)

SOURCES=tmc-spyware-server-cgi.c auth.c datastream.c settings.c
HEADERS=auth.h datastream.h settings.h

all: $(BINARY)

config.h:
	touch $@

$(BINARY): $(SOURCES) $(HEADERS)
	mkdir -p cgi-bin
	gcc -std=c99 -Wall -Os -D_FILE_OFFSET_BITS=64 $(DEP_CFLAGS) -o $@ $(SOURCES) $(DEP_LIBS)

clean:
	rm -f ${BINARY}
	if [ -e cgi-bin ]; then rmdir cgi-bin; fi
