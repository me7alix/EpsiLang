#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/api.h"
#include "../3dparty/cplus.h"

char *read_file(const char *filename);
bool write_to_file(const char *filename, const char *text);

EpslVal Exit(EpslEvalCtx *ctx, EpslLocation cloc, EpslVals args) {
	if (args.count != 1) {
		epsl_throw_error(ctx, cloc, "accepts only 1 argument");
		return EPSL_NONE;
	}

	if (args.items[0].kind != EPSL_VAL_INT) {
		epsl_throw_error(ctx, cloc, "accepts only integer");
		return EPSL_NONE;
	}

	exit(args.items[0].as.vint);
	return EPSL_NONE;
}

EpslVal System(EpslEvalCtx *ctx, EpslLocation cloc, EpslVals args) {
	if (args.count == 0) {
		epsl_throw_error(ctx, cloc, "arguments were not provided");
		return EPSL_NONE;
	}

	StringBuilder str = {0};
	for (size_t i = 0; i < args.count; i++) {
		if (args.items[i].kind != EPSL_VAL_STR) {
			epsl_throw_error(ctx, cloc, "accepts only strings");
			return EPSL_NONE;
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

EpslVal TextFileWrite(EpslEvalCtx *ctx, EpslLocation cloc, EpslVals args) {
	if (args.count != 2) {
		epsl_throw_error(ctx, cloc, "not enough arguments");
		return EPSL_NONE;
	}

	if (
		args.items[0].kind != EPSL_VAL_STR ||
		args.items[1].kind != EPSL_VAL_STR
	) {
		epsl_throw_error(ctx, cloc, "strings expected");
		return EPSL_NONE;
	}

	char *filename = epsl_val_get_str(args.items[0])->items;
	char *text = epsl_val_get_str(args.items[1])->items;

	return (EpslVal){
		.kind = EPSL_VAL_BOOL,
		.as.vbool = write_to_file(filename, text),
	};
}

EpslVal TextFileRead(EpslEvalCtx *ctx, EpslLocation cloc, EpslVals args) {
	if (args.count != 1) {
		epsl_throw_error(ctx, cloc, "1 argument expected");
		return EPSL_NONE;
	}

	if (args.items[0].kind != EPSL_VAL_STR) {
		epsl_throw_error(ctx, cloc, "string expected");
		return EPSL_NONE;
	}

	char *res = read_file(epsl_val_get_str(args.items[0])->items);
	if (!res) return EPSL_NONE;

	EpslVal txt = epsl_eval_make_value(ctx, EPSL_VAL_STR);
	epsl_val_set_str(txt, res);
	return txt;
}

void print_error(EpslCallStack *cs, EpslLocation loc, EpslErrorKind ek, char *msg) {
	size_t line_num = loc.line_num + 1;
	size_t char_num = loc.line_char - loc.line_start + 1;
	const char *err_type =
		ek == EPSL_ERROR_COMPTIME ? "comptime error:" :
		ek == EPSL_ERROR_RUNTIME  ? "runtime error:"  : "";

	printf("\n%s:%zu:%zu: %s %s\n", loc.file, line_num, char_num, err_type, msg);

	loc.line_char = loc.line_start;
	char err_ptr[512];
	size_t cnt = 0;

	while (*loc.line_char != '\n' && *loc.line_char != '\0'){
		printf("%c", *loc.line_char);
		if (cnt < char_num - 1) {
			if (*loc.line_char != '\t') {
				err_ptr[cnt++] = ' ';
			} else {
				err_ptr[cnt++] = '\t';
			}
		}

		loc.line_char++;
	}

	printf("\n");
	err_ptr[cnt++] = '^';
	err_ptr[cnt] = '\0';
	printf("%s\n", err_ptr);

	if (!cs) return;
	for (int i = (int)cs->count - 1; i >= 0; i--) {
		char *func_name = cs->items[i].name;
		EpslLocation loc = cs->items[i].loc;
		size_t ln = loc.line_num + 1;
		size_t cn = loc.line_char - loc.line_start + 1;
		printf(" at %s (%s:%zu:%zu)\n", func_name, loc.file, ln, cn);
	}
}

void print_usage() {
	printf(
		"Usage: [options] file [script args]\n"
		"Options:\n"
		"  -c     Program passed in as string\n"
		"  -ast   Print abstract syntax tree\n"
		"  -tok   Print tokens\n");
}

void reg_platform(EpslCtx *ctx) {
	EpslVal str = epsl_make_value(ctx, EPSL_VAL_STR);

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

	epsl_val_set_str(str, platform);
	epsl_reg_var(ctx, true, "_OS_", str);
}

int main(int argc, char *argv[]) {
	char *input_file = NULL;
	bool print_toks = false;
	bool print_ast  = false;
	bool cmd        = false;
	DA(char*) script_args = {0};

	if (argc == 1) {
		print_usage();
		return 0;
	}

	for (size_t i = 1; i < argc; i++) {
		if (input_file) {
			da_append(&script_args, argv[i]);
			continue;
		}

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

	EpslVal os_args = epsl_make_value(ctx, EPSL_VAL_LIST);
	da_foreach(char*, arg, &script_args) {
		EpslVal os_arg = epsl_make_value(ctx, EPSL_VAL_STR);
		epsl_val_set_str(os_arg, *arg);
		epsl_val_list_append(os_args, os_arg);
	}

	reg_platform(ctx);
	epsl_reg_var(ctx, true, "_OS_ARGS_", os_args);

	epsl_reg_func(ctx, "text_read",  TextFileRead);
	epsl_reg_func(ctx, "text_write", TextFileWrite);
	epsl_reg_func(ctx, "system",     System);
	epsl_reg_func(ctx, "exit",		 Exit);

	if (print_toks) epsl_print_tokens(ctx);
	else if (print_ast) epsl_print_ast(ctx);
	else {
		epsl_eval(ctx);
		epsl_free(ctx);
		free(ctx);
	}

	da_free(&script_args);
	return 0;
}
