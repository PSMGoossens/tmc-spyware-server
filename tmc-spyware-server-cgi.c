
// For strchrnul
#define _GNU_SOURCE

#include "auth.h"
#include "datastream.h"
#include "settings.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>

#define MAX_QUERY_STRING 4096
#define MAX_PATH_LEN 4096


/**
 * Checks whether the request was a POST request.
 */
static int is_method_post();

/**
 * Reads and parses CONTENT_LENGTH from the environment, returns -1 if not present or invalid.
 */
static ssize_t get_content_length();

/**
 * Reads the keys in the null-terminated array `keys` from the query string.
 * Stores them in `buf`, a buffer of size `bufsize`.
 * Stores a pointer to the i'th value in `bufptrs[i]` (but does not null-terminate it).
 * Returns 0 on failure.
 */
static int read_query_string(const char **keys, char *buf, size_t bufsize, char **bufptrs);

/**
 * Finds the value for `key` from the query string `qs` without any decoding.
 * Returns the length of the value (excluding null byte),
 * or -1 if not found or -2 if buf too small.
 */
static ssize_t get_qs_key(const char *qs, const char *key, char *buf, size_t bufsize);

/**
 * Ensures the data directory exists. Returns 0 on failure, 1 on success.
 */
static int make_datadir(const char *course_name);

/**
 * Streams STDIN to the correct file in a failsafe manner.
 */
static int save_incoming_data(const char *course_name, const char *username, ssize_t expected_length);

/**
 * Outputs the CGI Status header. Returns 0 on success, 1 on failure (to match main()).
 */
static int respond(int status, const char *reason);


static int is_method_post()
{
    const char *method = getenv("REQUEST_METHOD");
    return (method && strcmp(method, "POST") == 0);
}

static ssize_t get_content_length()
{
    const char *value = getenv("CONTENT_LENGTH");
    if (value && *value != '\0') {
        errno = 0;
        long res = strtol(value, NULL, 10);
        if (errno == 0 && res >= 0) {
            return (ssize_t)res;
        } else {
            return -1;
        }
    } else {
        return -1;
    }
}

static int read_query_string(const char **keys, char *buf, size_t bufsize, char **bufptrs)
{
    const char *qs = getenv("QUERY_STRING");
    if (!qs) {
        fprintf(stderr, "No QUERY_STRING.\n");
        return 0;
    }
    if (strlen(qs) + 1 > MAX_QUERY_STRING) {
        fprintf(stderr, "QUERY_STRING too long.\n");
        return 0;
    }

    for (int i = 0; keys[i] != NULL; ++i) {
        ssize_t len = get_qs_key(qs, keys[i], buf, bufsize);
        if (len <= 0) {
            fprintf(stderr, "Missing GET parameter: %s\n", keys[i]);
            return 0;
        }
        bufptrs[i] = buf;
        buf += len;
        bufsize -= len;
    }

    return 1;
}

static ssize_t get_qs_key(const char *qs, const char *key, char *buf, size_t bufsize)
{
    const char *p = qs;
    const char *k = key;

    while (*p != '\0') {
        if (*p == '&') {
            // new key
            k = key;
        } else if (k && *p == '=') {
            // key found
            const char *start = p + 1;
            const char *end = strchrnul(start, '&');
            size_t len = end - start + 1;
            if (bufsize < len) {
                return -2;
            }
            memcpy(buf, start, len);
            buf[len - 1] = '\0';
            return len;
        } else if (k && *p == *k) {
            // key matches so far
            k++;
        } else {
            k = NULL;
        }

        p++;
    }

    return -1;
}

static int make_datadir(const char *course_name)
{
    if (mkdir(settings.data_dir, 0777) == -1 && errno != EEXIST) {
        fprintf(stderr, "Failed to create top-level data directory at %s.\n", settings.data_dir);
        return 0;
    }

    char pathbuf[MAX_PATH_LEN];
    if (snprintf(pathbuf, MAX_PATH_LEN, "%s/%s", settings.data_dir, course_name) >= MAX_PATH_LEN) {
        fprintf(stderr, "Data dir path too long.\n");
        return 0;
    }

    if (mkdir(pathbuf, 0777) == -1 && errno != EEXIST) {
        fprintf(stderr, "Failed to create data directory at %s.\n", pathbuf);
        return 0;
    }

    return 1;
}

static int save_incoming_data(const char *course_name, const char *username, ssize_t expected_length)
{
    char index_path[MAX_PATH_LEN];
    char data_path[MAX_PATH_LEN];

    if (snprintf(index_path, MAX_PATH_LEN, "%s/%s/%s.idx", settings.data_dir, course_name, username) >= MAX_PATH_LEN) {
        fprintf(stderr, "Data file path too long.\n");
        return 0;
    }

    if (snprintf(data_path, MAX_PATH_LEN, "%s/%s/%s.dat", settings.data_dir, course_name, username) >= MAX_PATH_LEN) {
        fprintf(stderr, "Data file path too long.\n");
        return 0;
    }

    if (!store_data(index_path, data_path, STDIN_FILENO, expected_length)) {
        return 0;
    }

    return 1;
}


static int respond(int status, const char *reason)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    fprintf(
        stderr,
        "%lld.%04d %d %s\n",
        (long long)tv.tv_sec,
        (int)tv.tv_usec / 1000,
        status,
        getenv("REMOTE_ADDR")
    );

    int ok = (printf("Status: %d %s\nContent-Type: text/plain; charset=utf-8\n\n%d %s\n", status, reason, status, reason) >= 0);
    return ok ? 0 : 1;
}

int main()
{
    if (!init_settings_from_env()) {
        fprintf(stderr, "Exiting due to missing or invalid settings.\n");
        return respond(500, "Internal Server Error");
    }

    if (!is_method_post()) {
        fprintf(stderr, "Not a POST request.\n");
        return respond(405, "Method Not Allowed");
    }

    ssize_t content_length = get_content_length();

    char param_buf[MAX_QUERY_STRING];
    const char *param_keys[] = {
        "course_name",
        "username",
        "password",
        NULL
    };
    char *param_vals[3];

    if (!read_query_string(param_keys, param_buf, MAX_QUERY_STRING, param_vals)) {
        // already printed a log message
        return respond(400, "Bad Request");
    }

    const char *course_name = param_vals[0];
    const char *username = param_vals[1];
    const char *password = param_vals[2];

    char auth_qs[MAX_QUERY_STRING];
    if (snprintf(auth_qs, MAX_QUERY_STRING, "username=%s&password=%s", username, password) >= MAX_QUERY_STRING) {
        fprintf(stderr, "Auth query string too long.\n");
        return respond(400, "Bad Request");
    }

    if (!do_auth(auth_qs)) {
        // already printed a log message
        return respond(403, "Forbidden");
    }

    if (!make_datadir(course_name)) {
        // already printed a log message
        return respond(500, "Internal Server Error");
    }

    if (!save_incoming_data(course_name, username, content_length)) {
        fprintf(stderr, "Data transfer failed.\n");
        return respond(500, "Internal Server Error");
    }

    return respond(200, "OK");
}
