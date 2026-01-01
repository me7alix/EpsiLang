#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../include/parser.h"
#include "../include/lexer.h"
#include "../include/eval.h"
#include "../include/print.h"

char *read_file(const char *filename) {
	FILE* file = fopen(filename, "rb");
	if (!file) {
		return NULL;
	}

	fseek(file, 0, SEEK_END);
	long filesize = ftell(file);
	rewind(file);

	char *buffer = malloc(filesize + 1);
	size_t read_size = fread(buffer, 1, filesize, file);
	if (read_size != filesize) {
		free(buffer);
		fclose(file);
		return NULL;
	}

	buffer[filesize] = '\0';
	fclose(file);

	return buffer;
}

void print_usage() {
	printf(
		"Usage: [options] file\n"
		"Options:\n"
		"  -ast   Print abstract syntax tree\n"
		"  -tok   Print tokens\n");
}

void epsl_reg_stdlib(Parser *p, EvalCtx *ctx);

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

	char *code = read_file(input_file);
	if (!code) {
		fprintf(stderr, "File reading error...\n");
		return 1;
	}

	Lexer lexer = lexer_init(input_file, code);
	if (print_toks) lexer_print(lexer);
	Parser parser = {lexer};
	EvalCtx eval_ctx = {0};
	epsl_reg_stdlib(&parser, &eval_ctx);

	AST *ast = parse(&parser);
	if (print_ast) ast_print(ast, 0);
	if (!(print_ast || print_toks))
		eval(&eval_ctx, ast);
	return 0;
}
