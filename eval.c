#include "eval.h"

#include <assert.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>

#include "code.h"
#include "error.h"
#include "opcode.h"
#include "stream.h"

static jmp_buf error_jumpbuf;

#define CELLS_CHUNK_SIZE 128

struct cells_chunk {
	signed char cells[CELLS_CHUNK_SIZE - sizeof(void *) * 2];
	struct cells_chunk *prev_chunk, *next_chunk;
};

static_assert(sizeof(struct cells_chunk) == CELLS_CHUNK_SIZE,
	"sizeof(struct cells_chunk) != CELLS_CHUNK_SIZE");

typedef struct {
	struct cells_chunk *first_chunk;
} cells_t;

typedef struct {
	signed char *cell;
	const signed char *chunk_left, *chunk_right;
	struct cells_chunk *chunk;
} cells_iter_t;

static size_t cells_mem_max = 0, cells_mem_used = 0;

noreturn static void _cells_error_oom(void)
{
	hgbf_err_record("out of memory (%zu B / %zu B)", cells_mem_used, cells_mem_max);
	longjmp(error_jumpbuf, 1);
}

static void cells_init(cells_t *cells)
{
	cells_mem_used += sizeof(struct cells_chunk);
	struct cells_chunk *const new_chunk = malloc(sizeof(struct cells_chunk));
	memset(new_chunk->cells, 0, sizeof new_chunk->cells);
	new_chunk->prev_chunk = NULL;
	new_chunk->next_chunk = NULL;
	cells->first_chunk = new_chunk;
}

static void cells_destroy(cells_t *cells)
{
	for (struct cells_chunk *chunk = cells->first_chunk->prev_chunk; chunk; ) {
		struct cells_chunk *const prev_chunk = chunk->prev_chunk;
		free(chunk);
		chunk = prev_chunk;
	}
	for (struct cells_chunk *chunk = cells->first_chunk; chunk; ) {
		struct cells_chunk *const next_chunk = chunk->next_chunk;
		free(chunk);
		chunk = next_chunk;
	}
}

static cells_iter_t _cells_iter_make(struct cells_chunk *chunk)
{
	const cells_iter_t iter = {
		.cell = chunk->cells,
		.chunk_left = chunk->cells,
		.chunk_right = chunk->cells + (sizeof chunk->cells - 1),
		.chunk = chunk,
	};
	return iter;
}

static cells_iter_t cells_iter_make(cells_t *cells)
{
	return _cells_iter_make(cells->first_chunk);
}

static cells_iter_t _cells_iter_next_chunk(struct cells_chunk *chunk)
{
	struct cells_chunk *target_chunk = chunk->next_chunk;
	if (!target_chunk) {
		if (cells_mem_max && cells_mem_used + sizeof(struct cells_chunk) > cells_mem_max)
			_cells_error_oom();
		cells_mem_used += sizeof(struct cells_chunk);
		struct cells_chunk *const new_chunk = malloc(sizeof(struct cells_chunk));
		memset(new_chunk->cells, 0, sizeof new_chunk->cells);
		new_chunk->prev_chunk = target_chunk;
		new_chunk->next_chunk = NULL;
		chunk->next_chunk = new_chunk;
		target_chunk = new_chunk;
	}
	return _cells_iter_make(target_chunk);
}

static cells_iter_t _cells_iter_prev_chunk(struct cells_chunk *chunk)
{
	struct cells_chunk *target_chunk = chunk->prev_chunk;
	if (!target_chunk) {
		if (cells_mem_max && cells_mem_used + sizeof(struct cells_chunk) > cells_mem_max)
			_cells_error_oom();
		cells_mem_used += sizeof(struct cells_chunk);
		struct cells_chunk *const new_chunk = malloc(sizeof(struct cells_chunk));
		memset(new_chunk->cells, 0, sizeof new_chunk->cells);
		new_chunk->prev_chunk = NULL;
		new_chunk->next_chunk = target_chunk;
		chunk->prev_chunk = new_chunk;
		target_chunk = new_chunk;
	}
	return _cells_iter_make(target_chunk);
}

#define cells_iter_ref_cell(iter) \
	((iter).cell)

#define cells_iter_next(iter) \
do { \
	if ((iter).cell < (iter).chunk_right) \
		(iter).cell++; \
	else \
		(iter) = _cells_iter_next_chunk((iter).chunk); \
} while (false)

#define cells_iter_prev(iter) \
do { \
	if ((iter).cell > (iter).chunk_left) \
		(iter).cell--; \
	else \
		(iter) = _cells_iter_prev_chunk((iter).chunk); \
} while (false)

static int eval(
	const hgbf_code_t *code,
	hgbf_istream_t *input, hgbf_ostream_t *output,
	cells_t *cells)
{
	register const unsigned char *cp = code->bytes; // Code pointer.
	register cells_iter_t dp = cells_iter_make(cells); // Data pointer.

	while (true) {
		const unsigned char opcode = *cp++;

		switch (opcode) {
			union {
				int int_;
				ptrdiff_t offset;
			} tempval;

		case (unsigned char)HGBF_OP_NXT:
			cells_iter_next(dp);
			break;

		case (unsigned char)HGBF_OP_PRV:
			cells_iter_prev(dp);
			break;

		case (unsigned char)HGBF_OP_INC:
			(*cells_iter_ref_cell(dp))++;
			break;

		case (unsigned char)HGBF_OP_DEC:
			(*cells_iter_ref_cell(dp))--;
			break;

		case (unsigned char)HGBF_OP_OUT:
			tempval.int_ = hgbf_ostream_write1(
				output, (unsigned char)*cells_iter_ref_cell(dp));
			if (tempval.int_) {
				hgbf_err_record("output error");
				return -1;
			}
			break;

		case (unsigned char)HGBF_OP_IN:
			tempval.int_ = hgbf_istream_read1(input);
			if (tempval.int_ < 0) {
				hgbf_err_record("input error");
				return -1;
			}
			*cells_iter_ref_cell(dp) = (signed char)(unsigned char)tempval.int_;
			break;

		case (unsigned char)HGBF_OP_JFZ:
			tempval.offset = (ptrdiff_t)*(uint32_t *)cp;
			cp += 4;
			if (!*cells_iter_ref_cell(dp))
				cp += tempval.offset;
			break;

		case (unsigned char)HGBF_OP_JBN:
			tempval.offset = (ptrdiff_t)*(uint32_t *)cp;
			cp += 4;
			if (*cells_iter_ref_cell(dp))
				cp -= tempval.offset;
			break;

		case (unsigned char)HGBF_OP_HLT:
			return 0;

		default:
			hgbf_err_record("internal error: unkown opcode 0x%02x (CP=0x%02x)",
				opcode, (cp - 1 - code->bytes));
			return -1;
		}
	}
}

void hgbf_memmax(size_t size)
{
	cells_mem_max = size;
}

int hgbf_eval(const hgbf_code_t *code, hgbf_eval_io_t io)
{
	assert(code->bytes[code->length - 1] == (unsigned char)HGBF_OP_HLT);
	cells_t cells;
	cells_mem_used = 0;
	cells_init(&cells);
	int ret;
	if (!setjmp(error_jumpbuf))
		ret = eval(code, io.i, io.o, &cells);
	else
		ret = -1;
	cells_destroy(&cells);
	return ret;
}
