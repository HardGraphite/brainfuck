#include "stream.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static inline bool ptr_tagged(void *p)
{
	return (uintptr_t)p & 1;
}

static inline void *ptr_tag(void *p)
{
	return (void *)((uintptr_t)p | 1);
}

static inline void *ptr_untag(void *p)
{
	return (void *)((uintptr_t)p & ~(uintptr_t)1);
}

typedef struct {
	const char *current;
	const char *begin;
	const char *end;
} strview_t;

hgbf_istream_t *hgbf_istream_open_file(const char *path)
{
	FILE *const fp = fopen(path, "rb");
	assert(!ptr_tagged(fp));
	return (hgbf_istream_t *)fp;
}

hgbf_istream_t *hgbf_istream_open_mem(const char *str, size_t len)
{
	strview_t *const sv = malloc(sizeof(strview_t));
	sv->current = str;
	sv->begin = str;
	sv->end = str + len;
	assert(!ptr_tagged(sv));
	return ptr_tag(sv);
}

void hgbf_istream_close(hgbf_istream_t *stream)
{
	if (!ptr_tagged(stream)) {
		fclose((FILE *)stream);
		return;
	}

	strview_t *const sv = ptr_untag(stream);
	free(sv);
}

hgbf_istream_t *hgbf_stdin(void)
{
	assert(!ptr_tagged(stdin));
	return (hgbf_istream_t *)stdin;
}

int hgbf_istream_read1(hgbf_istream_t *stream)
{
	if (!ptr_tagged(stream))
		return fgetc((FILE *)stream);

	strview_t *const sv = ptr_untag(stream);
	assert(sv->current >= sv->begin && sv->current <= sv->end);
	if (sv->current < sv->end)
		return (int)(unsigned char)(*sv->current++);
	else
		return EOF;
}

hgbf_ostream_t *hgbf_ostream_open_file(const char *path)
{
	return (hgbf_ostream_t *)fopen(path, "wb");
}

void hgbf_ostream_close(hgbf_ostream_t *stream)
{
	fclose((FILE *)stream);
}

hgbf_ostream_t *hgbf_stdout(void)
{
	return (hgbf_ostream_t *)stdout;
}

int hgbf_ostream_write1(hgbf_ostream_t *stream, unsigned char data)
{
	return fputc((int)data, (FILE *)stream) != EOF ? 0 : -1;
}
