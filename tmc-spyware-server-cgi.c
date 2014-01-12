
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
 * Copies the environment variable to the given buffer.
 * Returns 0 on failure, 1 on success.
 */
static int copy_env(const char *name, char *buf, ssize_t bufsize);

/**
 * Ensures the data directory exists. Returns 0 on failure, 1 on success.
 */
static int make_datadir();

/**
 * Streams STDIN to the correct file in a failsafe manner.
 */
static int save_incoming_data(const char *username, ssize_t expected_length);

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

static int copy_env(const char *name, char *buf, ssize_t bufsize)
{
    const char *value = getenv(name);
    if (!value) {
        return 0;
    }
    size_t len = strlen(value);

    if (len + 1 > bufsize) {
        fprintf(stderr, "Env variable too long: %s\n", name);
        return 0;
    }

    memcpy(buf, value, len + 1);
    return 1;
}

static int make_datadir()
{
    if (mkdir(settings.data_dir, 0777) == -1 && errno != EEXIST) {
        fprintf(stderr, "Failed to create top-level data directory at %s.\n", settings.data_dir);
        return 0;
    }

    return 1;
}

static int save_incoming_data(const char *username, ssize_t expected_length)
{
    char index_path[MAX_PATH_LEN];
    char data_path[MAX_PATH_LEN];

    if (snprintf(index_path, MAX_PATH_LEN, "%s/%s.idx", settings.data_dir, username) >= MAX_PATH_LEN) {
        fprintf(stderr, "Data file path too long.\n");
        return 0;
    }

    if (snprintf(data_path, MAX_PATH_LEN, "%s/%s.dat", settings.data_dir, username) >= MAX_PATH_LEN) {
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

    char protocol_version[16];
    char username[256];
    char password[256];
    if (!copy_env("HTTP_X_TMC_VERSION", protocol_version, sizeof(protocol_version))) {
        fprintf(stderr, "Protocol version missing or too long.\n");
        return respond(400, "Bad Request");
    }

    if (strcmp(protocol_version, "1") != 0) {
        fprintf(stderr, "Unknown protocol version.\n");
        return respond(400, "Bad Request");
    }

    if (!copy_env("HTTP_X_TMC_USERNAME", username, sizeof(username))) {
        fprintf(stderr, "Username missing or too long.\n");
        return respond(400, "Bad Request");
    }
    if (!copy_env("HTTP_X_TMC_PASSWORD", password, sizeof(password))) {
        fprintf(stderr, "Password missing or too long.\n");
        return respond(400, "Bad Request");
    }

    // We use username as a file name so we need to be careful
    if (username[0] == '.') {
        fprintf(stderr, "Username starts with '.'.\n");
        return respond(400, "Bad Request");
    }
    if (strchr(username, '/')) {
        fprintf(stderr, "Username contains a '/'.\n");
        return respond(400, "Bad Request");
    }

    char auth_qs[MAX_QUERY_STRING];
    if (snprintf(auth_qs, MAX_QUERY_STRING, "username=%s&password=%s", username, password) >= MAX_QUERY_STRING) {
        fprintf(stderr, "Auth query string too long.\n");
        return respond(400, "Bad Request");
    }

    if (!do_auth(username, password)) {
        // already printed a log message
        return respond(403, "Forbidden");
    }

    if (!make_datadir()) {
        // already printed a log message
        return respond(500, "Internal Server Error");
    }

    if (!save_incoming_data(username, content_length)) {
        fprintf(stderr, "Data transfer failed.\n");
        return respond(500, "Internal Server Error");
    }

    return respond(200, "OK");
}
