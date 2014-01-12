
// For fmemopen
#define _POSIX_C_SOURCE 200809L

#include "settings.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <curl/curl.h>

#define URL_SIZE 4096
#define AUTHBUF_CAPACITY 100
#define AUTH_TIMEOUT 15

static int build_url(CURL *curl, char *urlbuf, size_t urlbufsize, const char *username, const char *password)
{
    char *escaped_username = curl_easy_escape(curl, username, 0);
    if (!escaped_username) {
        return 0;
    }
    char *escaped_password = curl_easy_escape(curl, password, 0);
    if (!escaped_password) {
        curl_free(escaped_username);
        return 0;
    }

    int ret = 1;
    if (snprintf(urlbuf, urlbufsize, "%s?username=%s&password=%s", settings.auth_url, escaped_username, escaped_password) >= urlbufsize) {
        fprintf(stderr, "Authentication URL too long.\n");
        ret = 0;
    }

    curl_free(escaped_username);
    curl_free(escaped_password);
    return ret;
}

int do_auth(const char *username, const char *password)
{
    int ret = 0;
    char urlbuf[URL_SIZE];
    char resultbuf[AUTHBUF_CAPACITY];
    char errbuf[CURL_ERROR_SIZE];

    FILE *f = fmemopen(resultbuf, AUTHBUF_CAPACITY - 1, "wb");
    if (!f) {
        fprintf(stderr, "Failed to open authbuf as a file.\n");
        return 0;
    }
    setbuf(f, NULL);

    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to open CURL.\n");
        fclose(f);
        return 0;
    }

    if (!build_url(curl, urlbuf, URL_SIZE, username, password)) {
        curl_easy_cleanup(curl);
        fclose(f);
        return 0;
    }

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
