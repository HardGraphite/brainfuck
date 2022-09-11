#include "getopt.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>

static const hgbf_optdef_t *find_opt(const hgbf_optdef_t opts[], char name)
{
	for (const hgbf_optdef_t *opt = opts; opt->name; opt++) {
		if (opt->name == name)
			return opt;
	}
	return NULL;
}

noreturn static void error_unknown_option(const char *prog, const char opt)
{
	fprintf(stderr, "%s: unrecognized command-line option `-%c'\n", prog, opt);
	exit(EXIT_FAILURE);
}

noreturn static void error_no_argument(const char *prog, const char opt)
{
	fprintf(stderr, "%s: command-line option `-%c' takes an argument\n", prog, opt);
	exit(EXIT_FAILURE);
}

void hgbf_getopt(
	const hgbf_optdef_t opts[], hgbf_getopt_handler_t handler,
	int argc, char *argv[], void *handler_param)
{
	assert(argc > 0 && !argv[argc]);
	for (int arg_index = 1; arg_index < argc; arg_index++) {
		const char *const arg = argv[arg_index];
		if (arg[0] != '-' || arg[1] == '\0') {
			handler(arg_index, NULL, arg, handler_param);
			continue;
		}
		for (size_t i = 1; ; i++) {
			const char name = arg[i];
			if (!name)
				break;
			const hgbf_optdef_t *const opt = find_opt(opts, name);
			if (!opt)
				error_unknown_option(argv[0], name);
			if (opt->arg) {
				if (arg[i + 1] || arg_index == argc - 1 || argv[arg_index + 1][0] == '-')
					error_no_argument(argv[0], name);
				handler(arg_index, opt, argv[arg_index + 1], handler_param);
				arg_index++;
				break;
			} else {
				handler(arg_index, opt, NULL, handler_param);
			}
		}
	}
}

void hgbf_opthelp(const hgbf_optdef_t opts[])
{
	for (const hgbf_optdef_t *opt = opts; opt->name; opt++) {
		char buffer[20];
		char *buffer_p = buffer;

		*buffer_p++ = ' ';
		*buffer_p++ = '-';
		*buffer_p++ = opt->name;
		if (opt->arg) {
			*buffer_p++ = ' ';
			const size_t n = strlen(opt->arg);
			memcpy(buffer_p, opt->arg, n);
			buffer_p += n;
		}
		assert(buffer_p - buffer < sizeof buffer);

		if (!opt->help) {
			*buffer_p++ = '\n';
			fwrite(buffer, 1, buffer_p - buffer, stdout);
			continue;
		}

		memset(buffer_p, ' ', sizeof buffer - (buffer_p - buffer));
		fwrite(buffer, 1, sizeof buffer, stdout);
		puts(opt->help);
	}
}
