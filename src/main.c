#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/api.h"
#include "../3dparty/cplus.h"

EpslVal Exit(EpslEvalCtx *ctx, EpslLocation call_loc, EpslVals args) {
	if (args.count != 1) {
		epsl_throw_error(ctx, call_loc, "exit() accepts only 1 argument");
		return EPSL_VNONE;
	}

	if (args.items[0].kind != EPSL_VAL_INT) {
		epsl_throw_error(ctx, call_loc, "exit() accepts only integer");
		return EPSL_VNONE;
	}

	exit(args.items[0].as.vint);
	return EPSL_VNONE;
}

EpslVal System(EpslEvalCtx *ctx, EpslLocation call_loc, EpslVals args) {
	if (args.count == 0) {
		epsl_throw_error(ctx, call_loc, "arguments were not provided");
		return EPSL_VNONE;
	}

	StringBuilder str = {0};
	for (size_t i = 0; i < args.count; i++) {
		if (args.items[i].kind != EPSL_VAL_STR) {
			epsl_throw_error(ctx, call_loc, "system() accepts only strings");
			return EPSL_VNONE;
		}

		char *arg_str = epsl_val_get_str(args.items[i])->items;
		sb_appendf(&str, "%s ", arg_str);
	}

	int res = system(str.items);
	sb_free(&str);

	return (EpslVal){
		.kind = EPSL_VAL_INT,
		.as.vint = res,
	};
}

void print_error(EpslLocation loc, EpslErrorKind ek, char *msg) {
	size_t lines_num = loc.line_num + 1;
	size_t chars_num = loc.line_char - loc.line_start + 1;
	const char *err_type =
		ek == EPSL_ERROR_COMPTIME ? "comptime error:" :
		ek == EPSL_ERROR_RUNTIME  ? "runtime error:"  : "";

	printf("\n%s:%zu:%zu: %s %s\n", loc.file, lines_num, chars_num, err_type, msg);

	loc.line_char = loc.line_start;
	char err_ptr[512];
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
		"  -c     Program passed in as string\n"
		"  -ast   Print abstract syntax tree\n"
		"  -tok   Print tokens\n");
}

void reg_platform(EpslCtx *ctx) {
	EpslVal str = epsl_new_heap_val(ctx, EPSL_VAL_STR);

	char *platform;
#if defined(_WIN32)
	platform = "WINDOWS";
#elif defined(__linux__)
	platform = "LINUX";
#elif defined(__APPLE__)
	platform = "APPLE";
#else
	platform = "NONE";
#endif

	epsl_val_set_str(ctx, str, platform);
	epsl_reg_var(ctx, "_OS_", str);
}

int main(int argc, char *argv[]) {
	char *input_file = NULL;
	bool print_toks = false;
	bool print_ast  = false;
	bool cmd        = false;

	if (argc == 1) {
		print_usage();
		return 0;
	}

	for (size_t i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-tok") == 0) {
			print_toks = true;
		} else if (strcmp(argv[i], "-ast") == 0) {
			print_ast = true;
		} else if (strcmp(argv[i], "-c") == 0) {
			cmd = true;
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

	EpslCtx *ctx;
	if (!cmd) {
		ctx = epsl_from_file(print_error, input_file);
		if (!ctx) {
			fprintf(stderr, "No such file...\n");
			return 1;
		}
	} else {
		ctx = epsl_from_str(print_error, input_file);
	}

	reg_platform(ctx);
	epsl_reg_func(ctx, "exit", Exit);
	epsl_reg_func(ctx, "system", System);

	if (print_toks) epsl_print_tokens(ctx);
	else if (print_ast) epsl_print_ast(ctx);
	else epsl_eval(ctx);
	return 0;
}
