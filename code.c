#include "code.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "opcode.h"
#include "stream.h"

#define CODEBUF_CHUNK_SIZE 128

struct codebuf_chunk {
	size_t length;
	unsigned char bytes[CODEBUF_CHUNK_SIZE - sizeof(size_t) - sizeof(void *)];
	struct codebuf_chunk *next_chunk;
};

static_assert(sizeof(struct codebuf_chunk) == CODEBUF_CHUNK_SIZE,
	"sizeof(struct codebuf_chunk) != CODEBUF_CHUNK_SIZE");

typedef struct {
	size_t length;
	struct codebuf_chunk *last_chunk;
	struct codebuf_chunk *chunks;
} codebuf_t;

static void _codebuf_add_chunk(codebuf_t *cb)
{
	struct codebuf_chunk *const new_chunk = malloc(sizeof(struct codebuf_chunk));
	new_chunk->length = 0;
	new_chunk->next_chunk = NULL;

	if (cb->last_chunk) {
		assert(cb->chunks);
		assert(!cb->last_chunk->next_chunk);
		cb->last_chunk->next_chunk = new_chunk;
	} else {
		assert(!cb->chunks);
		cb->length = 0;
		cb->chunks = new_chunk;
	}
	cb->last_chunk = new_chunk;
}

static void codebuf_init(codebuf_t *cb)
{
	cb->length = 0;
	cb->last_chunk = NULL;
	cb->chunks = NULL;
	_codebuf_add_chunk(cb);
}

static void codebuf_destroy(codebuf_t *cb)
{
	for (struct codebuf_chunk *chunk = cb->chunks; chunk; ) {
		struct codebuf_chunk *const next_chunk = chunk->next_chunk;
		free(chunk);
		chunk = next_chunk;
	}
}

static void codebuf_append1(codebuf_t *cb, unsigned char data)
{
	struct codebuf_chunk *current_chunk = cb->last_chunk;
	assert(current_chunk);
	assert(current_chunk->length <= sizeof current_chunk->bytes);
	if (current_chunk->length == sizeof current_chunk->bytes) {
		_codebuf_add_chunk(cb);
		current_chunk = cb->last_chunk;
	}
	current_chunk->bytes[current_chunk->length++] = data;
	cb->length++;
}

static void codebuf_append(codebuf_t *cb, const unsigned char *data, size_t size)
{
	struct codebuf_chunk *current_chunk = cb->last_chunk;
	assert(current_chunk);
	assert(current_chunk->length <= sizeof current_chunk->bytes);
	if (current_chunk->length + size > sizeof current_chunk->bytes) {
		_codebuf_add_chunk(cb);
		current_chunk = cb->last_chunk;
	}
	memcpy(current_chunk->bytes + current_chunk->length, data, size);
	current_chunk->length += size;
	cb->length += size;
}

static unsigned char *codebuf_ref(codebuf_t *cb, size_t index)
{
	assert(index < cb->length);
	const size_t off = cb->length - index;
	if (off <= cb->last_chunk->length)
		return cb->last_chunk->bytes + (cb->last_chunk->length - off);

	struct codebuf_chunk *chunk = cb->chunks;
	while (index >= chunk->length) {
		index -= chunk->length;
		chunk = chunk->next_chunk;
	}
	return chunk->bytes + index;
}

static void codebuf_copy(codebuf_t *cb, unsigned char *buffer)
{
	for (struct codebuf_chunk *chunk = cb->chunks;
			chunk; chunk = chunk->next_chunk) {
		const size_t n = chunk->length;
		memcpy(buffer, chunk->bytes, n);
		buffer += n;
	}
}

typedef struct {
	size_t *data;
	size_t size;
	size_t capacity;
} stack_t;

static void stack_init(stack_t *stack)
{
	const size_t n = 8;
	stack->capacity = n;
	stack->size = 0;
	stack->data = malloc(sizeof(size_t) * n);
}

static void stack_destroy(stack_t *stack)
{
	free(stack->data);
}

static bool stack_empty(stack_t *stack)
{
	return !stack->size;
}

static size_t stack_top(stack_t *stack)
{
	assert(!stack_empty(stack));
	return stack->data[stack->size - 1];
}

static void stack_push(stack_t *stack, size_t data)
{
	assert(stack->size <= stack->capacity);
	if (stack->size < stack->capacity)
		stack->data = realloc(stack->data, (stack->capacity *= 2));
	stack->data[stack->size++] = data;
}

static void stack_pop(stack_t *stack)
{
	assert(!stack_empty(stack));
	stack->size--;
}

static bool compile(hgbf_istream_t *source, stack_t *blocks, codebuf_t *code)
{
	size_t line_number = 1, column_number = 0;

	while (true) {
		const int c = hgbf_istream_read1(source);
		column_number++;

		switch (c) {
		case '>':
			codebuf_append1(code, (unsigned char)HGBF_OP_NXT);
			break;

		case '<':
			codebuf_append1(code, (unsigned char)HGBF_OP_PRV);
			break;

		case '+':
			codebuf_append1(code, (unsigned char)HGBF_OP_INC);
			break;

		case '-':
			codebuf_append1(code, (unsigned char)HGBF_OP_DEC);
			break;

		case '.':
			codebuf_append1(code, (unsigned char)HGBF_OP_OUT);
			break;

		case ',':
			codebuf_append1(code, (unsigned char)HGBF_OP_IN);
			break;

		case '[':
			codebuf_append1(code, (unsigned char)HGBF_OP_JFZ);
			stack_push(blocks, code->length);
			codebuf_append(code, (const unsigned char *)"\0\0\0", 4);
			break;

		case ']':
			codebuf_append1(code, (unsigned char)HGBF_OP_JBN);
			if (stack_empty(blocks)) {
				hgbf_err_record("%zu:%zu: no matching `[' for this `]'",
					line_number, column_number);
				return false;
			} else {
				const size_t pos = stack_top(blocks);
				stack_pop(blocks);
				assert(pos < code->length);
				const uint32_t off = (uint32_t)(code->length - pos);
				codebuf_append(code, (const unsigned char *)&off, 4);
				*(uint32_t *)codebuf_ref(code, pos) = off;
			}
			break;

		default:
			if (c < 0)
				return true;
			if (c == '\n')
				line_number++, column_number = 0;
			break;
		}
	}
}

hgbf_code_t *hgbf_code_compile(hgbf_istream_t *script)
{
	codebuf_t codebuf;
	stack_t blocks;
	codebuf_init(&codebuf);
	stack_init(&blocks);

	const bool ok = compile(script, &blocks, &codebuf);

	hgbf_code_t *code;
	if (!ok) {
		code = NULL;
	} else if (!stack_empty(&blocks)) {
		hgbf_err_record("`[' is not closed");
		code = NULL;
	} else {
		codebuf_append1(&codebuf, (unsigned char)HGBF_OP_HLT);
		code = malloc(sizeof(hgbf_code_t) + codebuf.length);
		code->length = codebuf.length;
		codebuf_copy(&codebuf, code->bytes);
	}

	codebuf_destroy(&codebuf);
	stack_destroy(&blocks);
	return code;
}

void hgbf_code_free(hgbf_code_t *code)
{
	free(code);
}
