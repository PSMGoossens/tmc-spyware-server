
#ifndef INC_SETTINGS_H
#define INC_SETTINGS_H

struct Settings {
    const char *data_dir;
    const char *auth_url;
};

extern struct Settings settings;

int init_settings_from_env();

#endif
