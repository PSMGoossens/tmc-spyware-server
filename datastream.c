
// For fdatasync
#define _POSIX_C_SOURCE 200809L

#include "datastream.h"
#include "site_index.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <openssl/sha.h>

#define STREAM_BUF_SIZE (32 * 1024)

typedef struct DataFiles {
    int index_fd;     // Index file FD opened for writing
    int data_fd;      // Data file FD opened for writing
    int is_new_file;  // 1 if the index file (and presumably data file) was just created
} DataFiles;

// All helper functions shall return non-zero on success.
static void to_hex(uint8_t* input, size_t len, char* output);  // output must be 2*len + 1 bytes long. Null-terminates output.
static char nibble_to_hex(uint8_t nibble);
static int create_or_open_datafiles(const char *index_path, const char *data_path, DataFiles *df);
static int stream_data(int infd, int outfd, off_t *total_written, ssize_t expected_length, SHA_CTX* hash_ctx);
static int write_index(int index_fd, off_t data_offset, off_t data_len, const char* sha1);
static void close_datafiles(DataFiles df);


int store_data(const char *data_dir, const char *index_path, const char *data_path, int infd, ssize_t expected_length)
{
    DataFiles df;
    if (!create_or_open_datafiles(index_path, data_path, &df)) {
        return 0;
    }

    off_t data_offset = lseek(df.data_fd, 0, SEEK_END);
    if (data_offset == (off_t)-1) {
        perror("Failed to find data file offset");
        goto fail;
    }

    SHA_CTX hash_ctx;
    uint8_t sha1_buf[SHA_DIGEST_LENGTH];
    char sha1_hex_buf[2*SHA_DIGEST_LENGTH + 1];
    if (SHA1_Init(&hash_ctx) == 0) {
        fprintf(stderr, "Failed to init SHA1 hash\n");
        goto fail;
    }

    off_t data_len = 0;
    int status = stream_data(infd, df.data_fd, &data_len, expected_length, &hash_ctx);
    if (SHA1_Final(sha1_buf, &hash_ctx) == 0) {
        fprintf(stderr, "Failed to finish SHA1 hash\n");
        goto fail;
    }
    if (!status) {
        goto fail;
    }

    if (fdatasync(df.data_fd) == -1) {
        perror("Failed to sync data file to disk");
        goto fail;
    }

    to_hex(sha1_buf, SHA_DIGEST_LENGTH, sha1_hex_buf);
    if (!write_index(df.index_fd, data_offset, data_len, sha1_hex_buf)) {
        goto fail;
    }

    if (fdatasync(df.index_fd) == -1) {
        perror("Failed to sync index file to disk");
        goto fail;
    }

    if (df.is_new_file) {
        write_site_index(data_dir);  // Ignore potential error. It was logged.
    }

    close_datafiles(df);
    return 1;

fail:
    close_datafiles(df);
    return 0;
}

static void to_hex(uint8_t* input, size_t len, char* output) {
    for (int i = 0; i < len; ++i) {
        uint8_t ch = input[i];
        uint8_t hi = (ch & 0xF0) >> 4;
        uint8_t lo = (ch & 0x0F);
        output[2*i] = nibble_to_hex(hi);
        output[2*i + 1] = nibble_to_hex(lo);
    }
    output[2*len] = '\0';
}

static char nibble_to_hex(uint8_t nibble) {
    if (nibble < 10) {
        return '0' + nibble;
    } else {
        return 'a' + (nibble - 10);
    }
}

static int create_or_open_datafiles(const char *index_path, const char *data_path, DataFiles *df)
{
    int index_fd = -1;
    int data_fd = -1;
    int olderrno;

    index_fd = open(index_path, O_CREAT | O_WRONLY, 0666);
    if (index_fd == -1) {
        perror("Failed to open index file");
        goto fail;
    }

    if (flock(index_fd, LOCK_EX) == -1) {
        perror("Failed to lock index file for writing");
        goto fail;
    }

    if (lseek(index_fd, 0, SEEK_END) == (off_t)(-1)) {
        perror("Failed to seek to end of index file");
        goto fail;
    }

    data_fd = open(data_path, O_CREAT | O_WRONLY | O_APPEND, 0666);
    if (data_fd == -1) {
        perror("Failed to open data file for appending");
        goto fail;
    }

    df->index_fd = index_fd;
    df->data_fd = data_fd;
    df->is_new_file = (lseek(index_fd, 0, SEEK_CUR) == (off_t)0);

    return 1;

fail:
    olderrno = errno;
    if (index_fd != -1) {
        flock(index_fd, LOCK_UN);
        close(index_fd);
    }
    if (data_fd != -1) {
        close(data_fd);
    }
    errno = olderrno;
    return 0;
}

static int stream_data(int infd, int outfd, off_t *total_written, ssize_t expected_length, SHA_CTX *hash_ctx)
{
    // Note: `expected_length` may be negative, which means there is no expected length from infd.

    char buf[STREAM_BUF_SIZE];

    *total_written = 0;
    while (expected_length != 0) {
        ssize_t amt_wanted;
        if (expected_length >= 0 && expected_length < STREAM_BUF_SIZE) {
            amt_wanted = expected_length;
        } else {
            amt_wanted = STREAM_BUF_SIZE;
        }

        ssize_t amt_read = read(infd, buf, amt_wanted);
        if (amt_read == -1) {
            perror("Reading incoming data failed");
            return 0;
        }
        if (amt_read == 0) {
            if (expected_length > 0) {
                fprintf(stderr, "Input was %lld bytes shorter than expected.\n", (long long)expected_length);
                return 0;
            } else {
                return 1;
            }
        }

        if (SHA1_Update(hash_ctx, buf, amt_read) == 0) {
            fprintf(stderr, "Hashing failed.\n");
            return 0;
        }

        ssize_t amt_written = 0;
        while (amt_written < amt_read) {
            ssize_t amt_written_now = write(outfd, buf + amt_written, amt_read - amt_written);
            if (amt_written_now == -1) {
                perror("Writing to data file failed");
                return 0;
            }
            amt_written += amt_written_now;
            *total_written += amt_written_now;
            if (expected_length >= 0) {
                expected_length -= amt_written_now;
            }
        }
    }

    return 1;
}

static int write_index(int index_fd, off_t data_offset, off_t data_len, const char* sha1_hex)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    const int buf_size = 256;
    char buf[buf_size];

    const char *remote_addr = getenv("HTTP_X_FORWARDED_FOR");
    if (remote_addr == NULL) {
        remote_addr = getenv("REMOTE_ADDR");
    }
    if (remote_addr == NULL) {
        remote_addr = "";
    }

    int rowlen = snprintf(
        buf,
        buf_size,
        "%lld %lld {\"ip\":\"%s\",\"t\":%lld,\"sha1\":\"%s\"}\n",
        (long long)data_offset,
        (long long)data_len,
        remote_addr,
        (long long)(tv.tv_sec * 1000 + tv.tv_usec / 1000),
        sha1_hex
    );

    errno = 0;
    ssize_t amt_written = write(index_fd, buf, rowlen);
    int ok = (amt_written == rowlen);
    if (!ok) {
        perror("Failed to write index");
    }
    return ok;
}

static void close_datafiles(DataFiles df)
{
    close(df.data_fd);
    flock(df.index_fd, LOCK_UN);
    close(df.index_fd);
}
