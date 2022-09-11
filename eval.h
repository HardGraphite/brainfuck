#pragma once

#include <stddef.h>

typedef struct hgbf_code hgbf_code_t;
typedef struct _hgbf_istream hgbf_istream_t;
typedef struct _hgbf_ostream hgbf_ostream_t;

// Evaluation I/O streams.
typedef struct {
	hgbf_istream_t *i;
	hgbf_ostream_t *o;
} hgbf_eval_io_t;

// Set cells memory limitation.
void hgbf_memmax(size_t size);

// Evaluate code. On success, return 0; on failure, return -1 and record error message.
int hgbf_eval(const hgbf_code_t *code, hgbf_eval_io_t io);
