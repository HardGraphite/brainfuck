#include "error.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static char *err_msg = NULL;
static size_t err_msg_cap = 0;

const char *hgbf_err_record(const char *fmt, ...)
{
	if (!err_msg) {
		err_msg_cap = 128;
		err_msg = malloc(err_msg_cap);
	}

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(err_msg, err_msg_cap, fmt, ap);
	va_end(ap);

	return hgbf_err_read();
}

const char *hgbf_err_read(void)
{
	return err_msg;
}

void hgbf_err_cleanup(void)
{
	if (err_msg) {
		free(err_msg);
		err_msg = NULL;
		err_msg_cap = 0;
	}
}
