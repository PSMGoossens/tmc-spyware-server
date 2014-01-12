
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

static int build_url(CURL *curl, char *urlbuf, size_t urlbuf_size, const char *username, const char *password, const char *session_id)
{
    int ret = 0;
    char *escaped_username = NULL;
    char *escaped_password = NULL;
    char *escaped_session_id = NULL;

    escaped_username = curl_easy_escape(curl, username, 0);
    if (!escaped_username) {
        goto done;
    }
    escaped_password = curl_easy_escape(curl, password, 0);
    if (!escaped_password) {
        goto done;
    }
    escaped_session_id = curl_easy_escape(curl, session_id, 0);
    if (!escaped_session_id) {
        goto done;
    }

    int url_len = snprintf(
        urlbuf,
        urlbuf_size,
        "%s?username=%s&password=%s&session_id=%s",
        settings.auth_url,
        escaped_username,
        escaped_password,
        escaped_session_id
    );
    if (url_len >= urlbuf_size) {
        fprintf(stderr, "Authentication URL too long.\n");
        goto done;
    }
    if (url_len < 0) {
        fprintf(stderr, "Failed to build authentication URL.\n");
        goto done;
    }

    ret = 1;

done:
    curl_free(escaped_username);
    curl_free(escaped_password);
    curl_free(escaped_session_id);
    return ret;
}

int do_auth(const char *username, const char *password, const char *session_id)
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

    if (!build_url(curl, urlbuf, URL_SIZE, username, password, session_id)) {
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
        fflush(f) == 0;

    if (ret) {
        ret = (strcmp(resultbuf, "OK") == 0);
        if (!ret) {
            fprintf(stderr, "Auth denied by server.\n");
        }
    } else {
        fprintf(stderr, "Auth failed: %s\n", errbuf);
    }

    curl_easy_cleanup(curl);
    fclose(f);
    return ret;
}
