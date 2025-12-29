#include <assert.h>
#include <stdio.h>
#include "../include/parser.h"
#include "../include/lexer.h"

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
	const int gap = 3;
	print_spaces(spaces);

	switch (n->kind) {
		case AST_PROG: {
			printf("prog:\n");
			ast_print(n->as.prog.body, spaces + gap);
		} break;

		case AST_BODY: {
			printf("body:\n");
			da_foreach (AST*, it, &n->as.body) {
				ast_print(*it, spaces + gap);
			}
		} break;

		case AST_LIT: {
			switch (n->as.lit.kind) {
				case LITERAL_INT:   printf("int(%lli)\n", n->as.lit.as.int_val);  break;
				case LITERAL_FLOAT: printf("float(%lf)\n", n->as.lit.as.float_val); break;
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
			printf("var_mut\n");
			ast_print(n->as.var_mut, spaces + gap);
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

		default: assert(0);
	}
}

int main(void) {
	Lexer lexer = lexer_init("main.grum", "a := 10; b := a + 6 / 2; a = a + b;");
	lexer_print(lexer);
	AST *ast = parser_parse(&lexer);
	ast_print(ast, 0);
	return 0;
}
