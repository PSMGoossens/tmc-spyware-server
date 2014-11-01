
BINARY=cgi-bin/tmc-spyware-server-cgi
CURL_CFLAGS=$(shell pkg-config --cflags libcurl)
CURL_LIBS=$(shell pkg-config --libs libcurl)
OPENSSL_CFLAGS=$(shell pkg-config --cflags openssl)
OPENSSL_LIBS=$(shell pkg-config --libs openssl)

DEP_CFLAGS=$(CURL_CFLAGS) $(OPENSSL_CFLAGS)
DEP_LIBS=$(CURL_LIBS) $(OPENSSL_LIBS)

SOURCES=tmc-spyware-server-cgi.c auth.c datastream.c settings.c site_index.c
HEADERS=auth.h datastream.h settings.h site_index.h

all: $(BINARY)

config.h:
	touch $@

$(BINARY): $(SOURCES) $(HEADERS)
	mkdir -p cgi-bin
	gcc -std=c99 -Wall -Os -D_FILE_OFFSET_BITS=64 $(DEP_CFLAGS) -o $@ $(SOURCES) $(DEP_LIBS)

clean:
	rm -f ${BINARY}
	if [ -e cgi-bin ]; then rmdir cgi-bin; fi
