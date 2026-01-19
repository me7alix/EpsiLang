#include <stdio.h>
#include <assert.h>
#include "../include/lexer.h"
#include "../include/parser.h"

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

	if (n == NULL) {
		printf("NULL\n");
		return;
	}

	size_t ln = n->loc.line_num + 1;
	size_t lc = n->loc.line_char - n->loc.line_start + 1;
	printf("[%zu:%zu] ", ln, lc);

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
				case LITERAL_INT:
					printf("int(%lli)\n", n->as.lit.as.vint);
					break;

				case LITERAL_FLOAT:
					printf("float(%lf)\n", n->as.lit.as.vfloat);
					break;

				case LITERAL_BOOL:
					printf("bool(%s)\n", n->as.lit.as.vbool ? "true" : "false");
					break;

				case LITERAL_STR:
					printf("str(%s)\n", n->as.lit.as.vstr);
					break;
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
			da_foreach (AST*, it, &n->as.func_def.args) {
				printf("%s", (*it)->as.var);
				if (it - n->as.func_def.args.items != n->as.func_def.args.count - 1)
					printf(", ");
			}
			printf("):\n");
			ast_print(n->as.func_def.body, spaces + gap);
		} break;

		case AST_BIN_EXPR: {
			int bop = n->as.bin_expr.op;
			printf("bin_expr(%s):\n",
				bop == AST_OP_EQ     ? "="  :
				bop == AST_OP_IS_EQ  ? "==" :
				bop == AST_OP_NOT_EQ ? "!=" :
				bop == AST_OP_AND    ? "&&" :
				bop == AST_OP_OR     ? "||" :
				bop == AST_OP_ADD    ? "+"  :
				bop == AST_OP_SUB    ? "-"  :
				bop == AST_OP_MUL    ? "*"  :
				bop == AST_OP_DIV    ? "/"  : "E");
			ast_print(n->as.bin_expr.lhs, spaces + gap);
			ast_print(n->as.bin_expr.rhs, spaces + gap);
		} break;

		case AST_UN_EXPR: {
			int op = n->as.un_expr.op;
			printf("un_expr(%s):\n",
				op == AST_OP_NOT    ? "!"  :
				op == AST_OP_NEG    ? "-"  : "E");
			ast_print(n->as.un_expr.v, spaces + gap);
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

		case AST_ST_IF: {
			printf("st_if:\n");
			ast_print(n->as.st_if_chain.cond, spaces + gap);
			ast_print(n->as.st_if_chain.body, spaces + gap);
			if (n->as.st_if_chain.chain)
				ast_print(n->as.st_if_chain.chain, spaces + gap);
		} break;

		case AST_ST_ELSE: {
			printf("st_else:\n");
			ast_print(n->as.st_else.body, spaces + gap);
		} break;

		case AST_ST_WHILE:{
			printf("st_while:\n");
			ast_print(n->as.st_while.cond, spaces + gap);
			ast_print(n->as.st_while.body, spaces + gap);
		} break;

		case AST_LIST: {
			printf("list:\n");
			da_foreach (AST*, it, &n->as.list)
				ast_print(*it, spaces + gap);
		} break;

		case AST_DICT: {
			printf("dict:\n");
			da_foreach (AST*, it, &n->as.list)
				ast_print(*it, spaces + gap);
		} break;

		case AST_ST_FOR: {
			printf("for:\n");
			ast_print(n->as.st_for.var, gap);
			ast_print(n->as.st_for.cond, gap);
			ast_print(n->as.st_for.mut, gap);
			ast_print(n->as.st_for.body, gap);
		} break;

		case AST_ST_FOREACH: {
			printf("foreach(%s):\n", n->as.st_foreach.var_id);
			ast_print(n->as.st_foreach.coll, gap);
			ast_print(n->as.st_foreach.body, gap);
		} break;

		case AST_BREAK: {
			printf("break");
		} break;

		case AST_CONT: {
			printf("continue");
		} break;

		default: assert(0);
	}
}
