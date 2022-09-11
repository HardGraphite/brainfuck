#pragma once

#include <stddef.h>

// Input stream.
typedef struct _hgbf_istream hgbf_istream_t;

// Output stream.
typedef struct _hgbf_ostream hgbf_ostream_t;

// Open an istream from file.
hgbf_istream_t *hgbf_istream_open_file(const char *path);

// Open an istream from immutable string.
hgbf_istream_t *hgbf_istream_open_mem(const char *str, size_t len);

// Close an istream.
void hgbf_istream_close(hgbf_istream_t *stream);

// Get standard input stream.
hgbf_istream_t *hgbf_stdin(void);

// Read one byte. Return -1 on failure.
int hgbf_istream_read1(hgbf_istream_t *stream);

// Open an ostream from file.
hgbf_ostream_t *hgbf_ostream_open_file(const char *path);

// Close an ostream.
void hgbf_ostream_close(hgbf_ostream_t *stream);

// Get standard output stream.
hgbf_ostream_t *hgbf_stdout(void);

// Write one byte. Return 0 on success or -1 on failure.
int hgbf_ostream_write1(hgbf_ostream_t *stream, unsigned char data);
