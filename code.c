#include "code.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
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

typedef enum {
	TOK_NXT, // '>'
	TOK_PRV, // '<'
	TOK_INC, // '+'
	TOK_DEC, // '-'
	TOK_OUT, // '.'
	TOK_IN,  // ','
	TOK_JFZ, // '['
	TOK_JBN, // ']'
	TOK_END,
} token_t;

typedef struct {
	token_t current_token;
	hgbf_istream_t *source;
	size_t line_number;
	size_t column_number;
} scanner_t;

static void _scanner_advance(scanner_t *scanner)
{
	while (true) {
		const int c = hgbf_istream_read1(scanner->source);
		scanner->column_number++;

		switch (c) {
		case '>':
			scanner->current_token = TOK_NXT;
			return;

		case '<':
			scanner->current_token = TOK_PRV;
			return;

		case '+':
			scanner->current_token = TOK_INC;
			return;

		case '-':
			scanner->current_token = TOK_DEC;
			return;

		case '.':
			scanner->current_token = TOK_OUT;
			return;

		case ',':
			scanner->current_token = TOK_IN;
			return;

		case '[':
			scanner->current_token = TOK_JFZ;
			return;

		case ']':
			scanner->current_token = TOK_JBN;
			return;

		default:
			if (c < 0) {
				scanner->current_token = TOK_END;
				return;
			}
			if (c == '\n') {
				scanner->line_number++;
				scanner->column_number = 0;
			}
			break;
		}
	}
}

static void scanner_init(scanner_t *scanner, hgbf_istream_t *source)
{
	scanner->source = source;
	scanner->line_number = 1;
	scanner->column_number = 0;
	_scanner_advance(scanner);
}

static token_t scanner_peek(scanner_t *scanner)
{
	return scanner->current_token;
}

static token_t scanner_next(scanner_t *scanner)
{
	const token_t token = scanner->current_token;
	_scanner_advance(scanner);
	return token;
}

static void scanner_drop(scanner_t *scanner)
{
	_scanner_advance(scanner);
}

static bool compile(hgbf_istream_t *source, stack_t *blocks, codebuf_t *code)
{
	scanner_t scanner;
	scanner_init(&scanner, source);

	while (true) {
		const token_t token = scanner_next(&scanner);

		switch (token) {
		case TOK_NXT:
		case TOK_PRV:
		case TOK_INC:
		case TOK_DEC:
		{
			size_t n = 1;
			while (scanner_peek(&scanner) == token) {
				scanner_drop(&scanner);
				n++;
			}

			if (n == 1) {
				static_assert(
					(int)HGBF_OP_NXT == (int)TOK_NXT &&
					(int)HGBF_OP_PRV == (int)TOK_PRV &&
					(int)HGBF_OP_INC == (int)TOK_INC &&
					(int)HGBF_OP_DEC == (int)TOK_DEC, "");
				codebuf_append1(code, (unsigned char)token);
			} else {
				static_assert(
					(int)HGBF_OP_NXTn == (int)TOK_NXT + 9 &&
					(int)HGBF_OP_PRVn == (int)TOK_PRV + 9 &&
					(int)HGBF_OP_INCn == (int)TOK_INC + 9 &&
					(int)HGBF_OP_DECn == (int)TOK_DEC + 9, "");
				codebuf_append1(code, (unsigned char)((int)token + 9));
				if (token <= TOK_PRV) {
					const uint16_t n_ = (uint16_t)n;
					codebuf_append(code, (const unsigned char *)&n, sizeof n_);
				} else {
					const uint8_t n_ = (uint16_t)n;
					codebuf_append(code, (const unsigned char *)&n, sizeof n_);
				}
			}
		}
			break;

		case TOK_OUT:
			codebuf_append1(code, (unsigned char)HGBF_OP_OUT);
			break;

		case TOK_IN:
			codebuf_append1(code, (unsigned char)HGBF_OP_IN);
			break;

		case TOK_JFZ:
			codebuf_append1(code, (unsigned char)HGBF_OP_JFZ);
			stack_push(blocks, code->length);
			codebuf_append(code, (const unsigned char *)"\0\0\0", 4);
			break;

		case TOK_JBN:
			codebuf_append1(code, (unsigned char)HGBF_OP_JBN);
			if (stack_empty(blocks)) {
				hgbf_err_record("%zu:%zu: no matching `[' for this `]'",
					scanner.line_number, scanner.column_number);
				return false;
			} else {
				const size_t pos = stack_top(blocks);
				stack_pop(blocks);
				assert(pos < code->length);
				const uint32_t off = (uint32_t)(code->length - pos);
				codebuf_append(code, (const unsigned char *)&off, sizeof off);
				*(uint32_t *)codebuf_ref(code, pos) = off;
			}
			break;

		case TOK_END:
			return true;

		default:
#if !defined NDEBUG
			abort();
#elif defined __GNUC__
			__builtin_unreachable();
#elif defined _MSC_VER
			__assume(0);
#else
			abort();
#endif
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

static const char *op_name[] = {
#define HGBF_OPCODE_LIST_ENTRY(NAME, CODE, OPRD) #NAME,
	HGBF_OPCODE_LIST
#undef HGBF_OPCODE_LIST_ENTRY
};

static const uint8_t operand_width[] = {
#define HGBF_OPCODE_LIST_ENTRY(NAME, CODE, OPRD) OPRD,
	HGBF_OPCODE_LIST
#undef HGBF_OPCODE_LIST_ENTRY
};

void hgbf_code_dump(const hgbf_code_t *code)
{
	for (const unsigned char *p = code->bytes,
			*const end = p + code->length; p < end; ) {
		const ptrdiff_t addr = p - code->bytes;
		const size_t opcode = *p++;
		if (opcode >= sizeof op_name / sizeof op_name[0])
			goto bad_opcode;
		const char *const name = op_name[opcode];
		const size_t oprd_wid = operand_width[opcode];
		if (!oprd_wid) {
			printf("%04tx: %s\n", addr, name);
			continue;
		}
		unsigned int operand;
		if (oprd_wid == 1)
			operand = *(const uint8_t *)p;
		else if (oprd_wid == 2)
			operand = *(const uint16_t *)p;
		else if (oprd_wid == 4)
			operand = *(const uint32_t *)p;
		else
			goto bad_opcode;
		p += oprd_wid;
		printf("%04tx: %-6s%u\n", addr, name, operand);
	}
	return;

bad_opcode:
	puts("???");
}

void hgbf_code_free(hgbf_code_t *code)
{
	free(code);
}
