
#include "settings.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct Settings settings;

#define SETTINGS_BUF_SIZE 4096

static char settings_buf[SETTINGS_BUF_SIZE];
static char *settings_buf_end = settings_buf;

static long read_str(const char *envvar, const char **ptr)
{
    const char *value = getenv(envvar);
    if (!value) {
        fprintf(stderr, "Settings %s too long.\n", envvar);
        return -1;
    }

    long len = strlen(value) + 1;
    long remaining = SETTINGS_BUF_SIZE - (settings_buf_end - settings_buf);

    if (len > remaining) {
        fprintf(stderr, "Settings %s too long.\n", envvar);
        return -2;
    }

    memcpy(settings_buf_end, value, len);
    *ptr = settings_buf_end;
    settings_buf_end += len;

    return len;
}

int init_settings_from_env()
{
    if (read_str("TMC_SPYWARE_DATA_DIR", &settings.data_dir) <= 0) {
        fprintf(stderr, "Missing envvar TMC_SPYWARE_DATA_DIR\n");
        return 0;
    }

    if (read_str("TMC_SPYWARE_AUTH_URL", &settings.auth_url) <= 0) {
        fprintf(stderr, "Missing envvar TMC_SPYWARE_AUTH_URL\n");
        return 0;
    }

    return 1;
}
