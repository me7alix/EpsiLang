#include <assert.h>
#include <math.h>
#include "../include/parser.h"
#include "../include/lexer.h"
#include "../include/exec.h"

char *read_file(const char *filename) {
	FILE* file = fopen(filename, "rb");
	if (!file) {
		return NULL;
	}

	fseek(file, 0, SEEK_END);
	long filesize = ftell(file);
	rewind(file);

	char *buffer = (char*) malloc(filesize + 1);
	if (!buffer) {
		fclose(file);
		return NULL;
	}

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

void lexer_print(Lexer l) {
	while (lexer_peek(&l).kind) {
		Token t = lexer_next(&l);
		printf("%02i %s\n", t.kind, t.data);
	}
}

void print_spaces(int spaces) {
	for (int i = 0; i < spaces; i++) printf(" ");
}

void ast_print(AST *n, int spaces) {
	const int gap = 2;
	print_spaces(spaces);

	switch (n->kind) {
		case AST_PROG: {
			printf("prog:\n");
			ast_print(n->as.prog.body, spaces + gap);
		} break;

		case AST_BODY: {
			printf("body:\n");
			da_foreach (AST*, it, &n->as.body)
			ast_print(*it, spaces + gap);
		} break;

		case AST_LIT: {
			switch (n->as.lit.kind) {
				case LITERAL_INT:   printf("int(%lli)\n", n->as.lit.as.int_val);    break;
				case LITERAL_FLOAT: printf("float(%lf)\n", n->as.lit.as.float_val); break;
				case LITERAL_BOOL:  printf("bool(%lf)\n", n->as.lit.as.float_val);  break;
			}
		} break;

		case AST_VAR: {
			printf("var(%s)\n", n->as.var);
		} break;

		case AST_VAR_DEF: {
			printf("var_def(%s):\n", n->as.var_def.id);
			ast_print(n->as.var_def.expr, spaces + gap);
		} break;

		case AST_VAR_MUT: {
			printf("var_mut:\n");
			ast_print(n->as.var_mut, spaces + gap);
		} break;

		case AST_FUNC_DEF: {
			printf("func_def(");
			da_foreach (char*, it, &n->as.func_def.args) {
				printf("%s", *it);
				if (it - n->as.func_def.args.items != n->as.func_def.args.count - 1)
					printf(", ");
			}
			printf("):\n");
			ast_print(n->as.func_def.body, spaces + gap);
		} break;

		case AST_BIN_EXPR: {
			int bop = n->as.bin_expr.op;
			printf("bin_expr(%s):\n",
				bop == AST_OP_EQ  ? "=" :
				bop == AST_OP_ADD ? "+" :
				bop == AST_OP_SUB ? "-" :
				bop == AST_OP_MUL ? "*" :
				bop == AST_OP_DIV ? "/" : "E");
			ast_print(n->as.bin_expr.lhs, spaces + gap);
			ast_print(n->as.bin_expr.rhs, spaces + gap);
		} break;

		case AST_RET: {
			printf("return:\n");
			ast_print(n->as.ret.expr, spaces + gap);
		} break;

		case AST_FUNC_CALL: {
			printf("func_call(%s):\n", n->as.func_call.id);
			da_foreach (AST*, it, &n->as.func_call.args)
			ast_print(*it, spaces + gap);
		} break;

		default: assert(0);
	}
}

void val_sprint(Val v, char *buf) {
	switch (v.type.kind) {
		case TYPE_INT:   sprintf(buf, "%lli", v.as.vint);  break;
		case TYPE_FLOAT: sprintf(buf, "%lf", v.as.vfloat); break;
		default:         sprintf(buf, "err");              break;
	}
}

Val fi_print(Vals args) {
	char buf[256];
	da_foreach (Val, it, &args) {
		val_sprint(*it, buf);
		printf("%s ", buf);
	}

	printf("\n");
	return (Val){0};
}

Val fi_sqrt(Vals args) {
	Val arg = da_get(&args, 0);
	double val =
		 arg.type.kind == TYPE_FLOAT ? arg.as.vfloat :
		 arg.type.kind == TYPE_INT   ? arg.as.vint   :
		 (assert(!"error: wrong args"), 0);

	return (Val){.type.kind = TYPE_FLOAT, .as.vfloat = sqrt(val)};
}

void reg_func(Parser *p, Exec *ex, RegFunc rf, char *name) {
	da_insert(&p->stack, 0, ((Symbol){
		.kind = SYMBOL_FUNC,
		.id = name,
		.as.func_args_cnt = 1,
	}));

	da_insert(&ex->stack, 0, ((ExecSymbol){
		.kind = EXEC_SYMB_REG_FUNC,
		.id = name,
		.as.reg_func = rf,
	}));
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

	Lexer lexer = lexer_init(input_file, read_file(input_file));
	if (print_toks) lexer_print(lexer);
	Parser parser = {lexer};
	Exec ex = {0};

	reg_func(&parser, &ex, fi_print, "print");
	reg_func(&parser, &ex, fi_sqrt,  "sqrt");

	AST *ast = parse(&parser);
	if (print_ast) ast_print(ast, 0);
	exec(&ex, ast);
	return 0;
}
