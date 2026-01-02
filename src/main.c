#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/api.h"

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

	EpslCtx *ctx = epsl_from_file(input_file);
	if (print_toks) epsl_print_tokens(ctx);
	else if (print_ast) epsl_print_ast(ctx);
	else epsl_eval(ctx);
	return 0;
}
