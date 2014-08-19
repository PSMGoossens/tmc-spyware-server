
#ifndef INC_DATASTREAM_H
#define INC_DATASTREAM_H

#include <stdio.h>  // for ssize_t

/**
 * Streams a batch of data from the FD `infd` to the given data file
 * in a fail-safe manner. Holds an exclusive write lock on the file.
 * If `expected_length` is >= 0 then no more than that amount is read
 * from `infd` and it is considered an error if there is less input.
 * Returns 0 on failure. May print error messages to stderr.
 */
int store_data(const char *data_dir, const char *index_path, const char *data_path, int infd, ssize_t expected_length);

#endif
