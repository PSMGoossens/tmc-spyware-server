
#ifndef INC_DATASTREAM_H
#define INC_DATASTREAM_H

/**
 * Streams a batch of data from the FD `infd` to the given data file
 * in a fail-safe manner. Holds an exclusive write lock on the file.
 * Returns 0 on failure. May print error messages to stderr.
 */
int store_data(const char *index_path, const char *data_path, int infd);

#endif
