
#define _POSIX_C_SOURCE 200809L  // for fdopen()
#define _BSD_SOURCE  // for scandir() and alphasort()

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>

#define MAX_PATH_LEN 4096

const char* index_file_ext = ".idx";

static void free_dents(struct dirent **dents, int num_dents)
{
    for (int i = 0; i < num_dents; ++i) {
        free(dents[i]);
    }
    free(dents);
}

static int ends_with(const char *s, const char *end)
{
    size_t slen = strlen(s);
    size_t endlen = strlen(end);
    return slen >= endlen && strcmp(&s[slen - endlen], end) == 0;
}

static int filter_dents(const struct dirent *dent)
{
    return ends_with(dent->d_name, index_file_ext);
}

int write_site_index(const char *dirname)
{
    char temp_path[MAX_PATH_LEN];
    char actual_path[MAX_PATH_LEN];
    int tempfile_exists = 0;
    int fd = -1;
    FILE *f = NULL;
    struct dirent **dents = NULL;
    int num_dents = 0;
    
    if (
        snprintf(actual_path, MAX_PATH_LEN, "%s/index.txt", dirname) >= MAX_PATH_LEN ||
        snprintf(temp_path, MAX_PATH_LEN, "%s/.index.txt.tmp.XXXXXX", dirname) >= MAX_PATH_LEN
    ) {
        fprintf(stderr, "Cannot write site index - path name too long.\n");
        goto fail;
    }

    fd = mkstemp(temp_path);
    if (fd == -1) {
        perror("Failed to open temporary site index file");
        goto fail;
    }
    tempfile_exists = 1;

    f = fdopen(fd, "wb");
    if (f == NULL) {
        perror("Failed to write site index - fdopen() failed");
        goto fail;
    }
    fd = -1;  // Now owned by `f`

    num_dents = scandir(dirname, &dents, &filter_dents, &alphasort);
    if (num_dents == -1) {
        perror("Failed to write site index - directory scan failed");
        goto fail;
    }
    
    for (int i = 0; i < num_dents; ++i) {
        char entry[MAX_PATH_LEN];
        size_t len = strlen(dents[i]->d_name);
        if (len < MAX_PATH_LEN && len > strlen(index_file_ext)) {
            strncpy(entry, dents[i]->d_name, MAX_PATH_LEN);
            entry[strlen(entry) - strlen(index_file_ext)] = '\0';
            if (fprintf(f, "%s\n", entry) < 0) {
                perror("Failed to write site index - fprintf() failed");
                goto fail;
            }
        } else {
            fprintf(stderr, "Skipping weird file name while writing site index: %s\n", dents[i]->d_name);
        }
    }
    
    free_dents(dents, num_dents);
    dents = NULL;
    
    int close_ok = (fclose(f) == 0);
    f = NULL;
    if (!close_ok) {
        perror("Failed to write site index - fclose() failed");
        goto fail;
    }
    
    if (rename(temp_path, actual_path) == -1) {
        perror("Failed to write site index - rename() failed");
        goto fail;
    }
    tempfile_exists = 0;
    
    return 1;
    
fail:
    if (tempfile_exists) {
        if (unlink(temp_path) == -1) {
            perror("Failed to delete temporary file while writing site index");
        }
    }
    if (f != NULL) {
        fclose(f);
    }
    if (fd != -1) {
        close(fd);
    }
    if (dents != NULL) {
        free_dents(dents, num_dents);
    }
    return 0;
}
