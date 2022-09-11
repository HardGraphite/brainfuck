#pragma once

#include <stdbool.h>

// Command line option definition.
typedef struct {
	char name;
	const char *arg;
	const char *help;
} hgbf_optdef_t;

// Callback function for `hgbf_getopt()`.
typedef void (*hgbf_getopt_handler_t)
	(int index, const hgbf_optdef_t *opt, const char *arg, void *param);

// Parse command line options.
void hgbf_getopt(
	const hgbf_optdef_t opts[], hgbf_getopt_handler_t handler,
	int argc, char *argv[], void *handler_param);

// Print help message for options ot stdout.
void hgbf_opthelp(const hgbf_optdef_t opts[]);
