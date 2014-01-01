
// For fmemopen
#define _POSIX_C_SOURCE 200809L

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <curl/curl.h>

#define URL_SIZE 4096
#define AUTHBUF_CAPACITY 100

int do_auth(const char *query_string)
{
    int ret = 0;
    char urlbuf[URL_SIZE];
    char resultbuf[AUTHBUF_CAPACITY];
    char errbuf[CURL_ERROR_SIZE];

    if (snprintf(urlbuf, URL_SIZE, "%s?%s", AUTH_URL, query_string) >= URL_SIZE) {
        fprintf(stderr, "Authentication URL too long.\n");
        return 0;
    }

    FILE *f = fmemopen(resultbuf, AUTHBUF_CAPACITY - 1, "wb");
    if (!f) {
        fprintf(stderr, "Failed to open authbuf as a file.\n");
        return 0;
    }
    setbuf(f, NULL);
    
    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_URL, urlbuf);
    curl_easy_setopt(curl, CURLOPT_NETRC, CURL_NETRC_IGNORED);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)(AUTH_TIMEOUT));
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    ret =
        curl_easy_perform(curl) == 0 &&
        fputc('\0', f) != EOF &&
        fflush(f) == 0 &&
        (strcmp(resultbuf, "OK") == 0);

    if (!ret) {
        fprintf(stderr, "Auth failed: %s\n", errbuf);
    }
    
    curl_easy_cleanup(curl);
    fclose(f);
    return ret;
}
