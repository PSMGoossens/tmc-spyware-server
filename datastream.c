
// For fdatasync
#define _POSIX_C_SOURCE 200809L

#include "datastream.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define STREAM_BUF_SIZE (32 * 1024)

typedef struct DataFiles {
    int index_fd; // Index file FD opened for writing
    int data_fd;  // Data file FD opened for writing
} DataFiles;

// All helper functions shall return non-zero on success.
static int create_or_open_datafiles(const char *index_path, const char *data_path, DataFiles *df);
static int stream_data(int infd, int outfd, off_t *total_written);
static int write_index(int index_fd, off_t data_offset, off_t data_len);
static void close_datafiles(DataFiles df);


int store_data(const char *index_path, const char *data_path, int infd)
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

    off_t data_len = 0;
    if (!stream_data(infd, df.data_fd, &data_len)) {
        goto fail;
    }

    if (fdatasync(df.data_fd) == -1) {
        perror("Failed to sync data file to disk");
        goto fail;
    }

    if (!write_index(df.index_fd, data_offset, data_len)) {
        goto fail;
    }

    if (fdatasync(df.index_fd) == -1) {
        perror("Failed to sync index file to disk");
        goto fail;
    }

    close_datafiles(df);
    return 1;

fail:
    close_datafiles(df);
    return 0;
}

static int create_or_open_datafiles(const char *index_path, const char *data_path, DataFiles *df)
{
    int index_fd = -1;
    int data_fd = -1;
    int olderrno;

    index_fd = open(index_path, O_CREAT | O_WRONLY, 0777);
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

    data_fd = open(data_path, O_CREAT | O_WRONLY | O_APPEND, 0777);
    if (data_fd == -1) {
        perror("Failed to open data file for appending");
        goto fail;
    }

    df->index_fd = index_fd;
    df->data_fd = data_fd;

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

static int stream_data(int infd, int outfd, off_t *total_written)
{
    char buf[STREAM_BUF_SIZE];

    *total_written = 0;
    while (1) {
        ssize_t amt_read = read(infd, buf, STREAM_BUF_SIZE);
        if (amt_read == -1) {
            perror("Reading incoming data failed");
            return 0;
        }
        if (amt_read == 0) {
            return 1;
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
        }
    }
}

static int write_index(int index_fd, off_t data_offset, off_t data_len)
{
    const int buf_size = 256; // Certainly enough for two textual 64-bit numbers
    char buf[buf_size];

    int rowlen = snprintf(
        buf,
        buf_size,
        "%lld %lld\n",
        (long long)data_offset,
        (long long)data_len
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
