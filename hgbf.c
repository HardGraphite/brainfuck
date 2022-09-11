#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined _WIN32
#	include <io.h>
#	include <locale.h>
#	include <Windows.h>
#else // !_WIN32
#	include <unistd.h>
#endif // _WIN32

#include "code.h"
#include "error.h"
#include "eval.h"
#include "getopt.h"
#include "stream.h"

typedef struct {
	const char *program;
	const char *script_file;
	const char *script_string;
	const char *istream_file;
	const char *ostream_file;
	size_t memory_limit;
	bool interactive;
} argparse_res_t;

static void init(void);
static argparse_res_t parse_args(int argc, char *argv[]);

int main(int argc, char *argv[])
{
	init();

	const argparse_res_t args = parse_args(argc, argv);
	if (args.memory_limit)
		hgbf_memmax(args.memory_limit);

	if (args.interactive) {
		fprintf(stderr, "%s: interactive mode has not been implemented\n",
			args.program);
		return EXIT_FAILURE;
	} else {
		hgbf_istream_t *const script =
			args.script_file ? !strcmp(args.script_file, "-") ?
				hgbf_stdin() : hgbf_istream_open_file(args.script_file) :
				hgbf_istream_open_mem(args.script_string, strlen(args.script_string));
		if (!script) {
			fprintf(stderr, "%s: failed to read the script\n", args.program);
			return EXIT_FAILURE;
		}
		hgbf_code_t *const code = hgbf_code_compile(script);
		hgbf_istream_close(script);
		if (!code) {
			fprintf(stderr, "%s: syntax error: %s\n", args.program, hgbf_err_read());
			return EXIT_FAILURE;
		}

		const hgbf_eval_io_t eval_io = {
			.i = !args.istream_file ? hgbf_stdin() :
				hgbf_istream_open_file(args.istream_file),
			.o = !args.ostream_file ? hgbf_stdout() :
				hgbf_ostream_open_file(args.ostream_file),
		};
		if (!eval_io.i) {
			fprintf(stderr, "%s: failed to open input stream\n", args.program);
			return EXIT_FAILURE;
		}
		if (!eval_io.o) {
			fprintf(stderr, "%s: failed to open output stream\n", args.program);
			return EXIT_FAILURE;
		}
		const int eval_err = hgbf_eval(code, eval_io);
		if (args.istream_file)
			hgbf_istream_close(eval_io.i);
		if (args.ostream_file)
			hgbf_ostream_close(eval_io.o);
		hgbf_code_free(code);
		if (eval_err) {
			fprintf(stderr, "%s: runtime error: %s\n", args.program, hgbf_err_read());
			return EXIT_FAILURE;
		}
	}
}

static void init(void)
{
#if defined _WIN32
	setlocale(LC_ALL, ".UTF-8");

	const UINT CODEPAGE_UTF8 = 65001U;
	SetConsoleCP(CODEPAGE_UTF8);
	SetConsoleOutputCP(CODEPAGE_UTF8);

	DWORD dwMode = 0;
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut != INVALID_HANDLE_VALUE) {
		if (GetConsoleMode(hOut, &dwMode)) {
			dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
			SetConsoleMode(hOut, dwMode);
		}
	}
#endif // _WIN32
}

static size_t parse_num_with_suffix(const char *s)
{
	char *endptr;
	unsigned long long num = strtoull(s, &endptr, 0);
	if (endptr == s)
		return (size_t)-1;
	if (*endptr) {
		const char c = endptr[0];
		if (c == 'K')
			num *= 1000;
		else if (c == 'M')
			num *= 1000 * 1000;
		else if (c == 'G')
			num *= 1000 * 1000 * 1000;
		else
			return (size_t)-1;
		if (endptr[1] && endptr[1] != 'i')
			return (size_t)-1;
	}
	return (size_t)num;
}

#pragma pack(push, 1)
static const hgbf_optdef_t optdefs[] = {
	{'h', NULL, "print help message and exit"},
	{'V', NULL, "print version information and exit"},
	{'e', "SCRIPT", "execute the SCRIPT string"},
	{'f', "FILE", "execute code from FILE"},
	{'i', NULL, "enter interactive mode"},
	{'I', "FILE", "use the FILE instead of stdin as input stream"},
	{'O', "FILE", "use the FILE instead of stdout as output stream"},
	{'M', "SIZE[K|M|G][i]", "maximum cells (runtime memory) size"},
	{0, NULL, NULL},
};
#pragma pack(pop)

static void getopt_handler(
	int index, const hgbf_optdef_t *opt, const char *arg, void *param)
{
	argparse_res_t *const res = param;

	if (!opt)
		goto opt_f;

	switch (opt->name) {
	case 'h':
		fputs(
			"Usage: hgbf [OPTION...] [FILE]" "\n"
			" HardGraphite's Brainfuck interpreter." "\n"
			"\n"
			"Options:" "\n"
		, stdout);
		hgbf_opthelp(optdefs);
		exit(EXIT_SUCCESS);

	case 'V':
#ifdef HGBF_VERSION
		puts(HGBF_VERSION);
#endif // HGBF_VERSION
		exit(EXIT_SUCCESS);

	case 'e':
		if (res->script_file) {
			fprintf(stderr, "%s: more than one script string\n",
				res->program);
			exit(EXIT_FAILURE);
		}
		if (res->script_file || res->interactive) {
			fprintf(stderr,
				"%s: options `-e', `-f' and `-i' are mutually exclusive\n",
				res->program);
			exit(EXIT_FAILURE);
		}
		res->script_string = arg;
		break;

	case 'f':
	opt_f:
		if (res->script_file) {
			fprintf(stderr, "%s: more than one script files: %s and %s\n",
				res->program, res->script_file, arg);
			exit(EXIT_FAILURE);
		}
		if (res->script_string || res->interactive) {
			fprintf(stderr,
				"%s: options `-e', `-f' and `-i' are mutually exclusive\n",
				res->program);
			exit(EXIT_FAILURE);
		}
		res->script_file = arg;
		break;

	case 'i':
		if (res->script_file || res->script_string) {
			fprintf(stderr,
				"%s: options `-e', `-f' and `-i' are mutually exclusive\n",
				res->program);
			exit(EXIT_FAILURE);
		}
		res->interactive = true;
		break;

	case 'I':
		res->istream_file = arg;
		break;

	case 'O':
		res->ostream_file = arg;
		break;

	case 'M':
		res->memory_limit = parse_num_with_suffix(arg);
		if (res->memory_limit == (size_t)-1) {
			fprintf(stderr, "%s: illegal size: `%s'\n",
				res->program, arg);
			exit(EXIT_FAILURE);
		}
		break;

	default:
		break;
	}
}

static bool stdin_is_tty(void)
{
#if defined _WIN32
	return _isatty(_fileno(stdin));
#else // !_WIN32
	return isatty(STDIN_FILENO);
#endif // _WIN32
}

static argparse_res_t parse_args(int argc, char *argv[])
{
	argparse_res_t res = {
		.program = argv[0],
		.script_file = NULL,
		.script_string = NULL,
		.istream_file = NULL,
		.ostream_file = NULL,
		.memory_limit = 0,
		.interactive = false,
	};
	hgbf_getopt(optdefs, getopt_handler, argc, argv, &res);
	if (!(res.script_file || res.script_string || res.interactive)) {
		if (stdin_is_tty())
			res.interactive = true;
		else
			res.script_file = "-";
	}
	return res;
}
