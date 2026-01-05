#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/api.h"

void print_error(EpslLocation loc, EpslErrorKind ek, char *msg) {
	size_t lines_num = loc.line_num + 1;
	size_t chars_num = loc.line_char - loc.line_start + 1;
	const char *err_type =
		ek == EPSL_ERROR_COMPTIME ? "comptime error:" :
		ek == EPSL_ERROR_RUNTIME  ? "runtime error:"  : "";

	printf("\n%s:%zu:%zu: %s %s\n", loc.file, lines_num, chars_num, err_type, msg);

	loc.line_char = loc.line_start;
	char err_ptr[128];
	size_t cnt = 0;

	while (*loc.line_char != '\n' && *loc.line_char != '\0'){
		printf("%c", *loc.line_char);
		if (cnt < chars_num - 1) {
			if (*loc.line_char != '\t')
				err_ptr[cnt++] = ' ';
			else
				err_ptr[cnt++] = '\t';
		}

		loc.line_char++;
	}

	printf("\n");
	err_ptr[cnt++] = '^';
	err_ptr[cnt] = '\0';
	printf("%s\n", err_ptr);
}

void print_usage() {
	printf(
		"Usage: [options] file\n"
		"Options:\n"
		"  -ast   Print abstract syntax tree\n"
		"  -tok   Print tokens\n");
}

int main(int argc, char *argv[]) {
	char *input_file = NULL;
	bool print_toks = false;
	bool print_ast  = false;

	if (argc == 1) {
		print_usage();
		return 0;
	}

	for (size_t i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-tok") == 0) {
			print_toks = true;
		} else if (strcmp(argv[i], "-ast") == 0) {
			print_ast = true;
		} else if (
			strcmp(argv[i], "-h") == 0 ||
			strcmp(argv[i], "--help") == 0) {
			print_usage();
			return 0;
		} else {
			if (argv[i][0] == '-') {
				fprintf(stderr, "invalid option %s\n", argv[i]);
				return 1;
			}

			input_file = argv[i];
		}
	}

	if (!input_file) {
		print_usage();
		return 0;
	}

	EpslCtx *ctx = epsl_from_file(print_error, input_file);
	if (!ctx) {
		fprintf(stderr, "No such file...\n");
		return 1;
	}

	if (print_toks) epsl_print_tokens(ctx);
	else if (print_ast) epsl_print_ast(ctx);
	else epsl_eval(ctx);
	return 0;
}
