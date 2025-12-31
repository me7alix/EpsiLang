#include <assert.h>
#include <stdbool.h>
#include <sys/types.h>
#include "../include/parser.h"

#define ast(...) ast_alloc((AST)__VA_ARGS__)
AST *ast_alloc(AST ast) {
	AST *n = malloc(sizeof(AST));
	*n = ast;
	return n;
}

void expect(Parser *p, TokenKind tk) {
	if (peek(p).kind != tk)
		lexer_error(peek(p).loc, "error: unexpected token");
}

void symbol_stack_add(SymbolStack *ss, Symbol s) {
	da_append(ss, s);
}

Symbol *symbol_stack_get(SymbolStack *ss, Token var) {
	for (ssize_t i = ss->count - 1; i >= 0; i--) {
		if (strcmp(da_get(ss, i).id, var.data) == 0) {
			return &da_get(ss, i);
		}
	}

	lexer_error(var.loc, "error: no such symbol at the scope");
	return NULL;
}

double parse_float(char *data) {
	return atof(data);
}

long long parse_int(char *data) {
	char *end;
	return strtoll(data, &end, 0);
}

uint ast_op_precedence(AST_Op op, bool l) {
	switch (op) {
		case AST_OP_EQ:
			return l ? 10 : 11;
		case AST_OP_SUB:
		case AST_OP_ADD:
			return l ? 21 : 20;
		case AST_OP_MUL:
		case AST_OP_DIV:
			return l ? 31 : 30;
		default: return 0;
	}
}

extern void ast_print(AST *n, int spaces);

AST *parse_expand(ASTs *nodes) {
	for (size_t i = 0; i < nodes->count; i++) {
		if (nodes->count == 1)
			return da_get(nodes, 0);

		AST *node = da_get(nodes, i);

		bool nlhs = node->as.bin_expr.lhs;
		bool nrhs = node->as.bin_expr.rhs;
		if (!(node->kind == AST_BIN_EXPR && (!nlhs || !nrhs))) {
			uint lpr = 0, rpr = 0;

			if (i > 0)
				lpr = ast_op_precedence(da_get(nodes, i - 1)->as.bin_expr.op, false);

			if (i < nodes->count - 1)
				rpr = ast_op_precedence(da_get(nodes, i + 1)->as.bin_expr.op, true);

			if (lpr == 0 && rpr == 0)
				lexer_error(node->loc, "error: wrong expression");

			if (lpr > rpr) da_get(nodes, i - 1)->as.bin_expr.rhs = node;
			else           da_get(nodes, i + 1)->as.bin_expr.lhs = node;

			da_remove_ordered(nodes, i);
			i--;
		}
	}

	return parse_expand(nodes);
}

typedef enum {
	PARSE_EXPR_SEMI,
	PARSE_EXPR_CPAR,
	PARSE_EXPR_ARGS,
} ParseExprKind;

AST *parse_expr(Parser *p, ParseExprKind pek);

AST *parse_func_call(Parser *p) {
	AST *fc = ast({
		.kind = AST_FUNC_CALL,
		.loc = p->lexer.cur_loc,
		.as.func_call.id = next(p).data,
	});

	next(p);
	int par_cnt = 1;

	for (;;) {
		switch (peek(p).kind) {
			case TOK_COM: {
				if(peek2(p).kind == TOK_COM)
					lexer_error(peek(p).loc, "error: too many coms");
				next(p);
			} break;

			case TOK_OPAR: {
				par_cnt++;
				next(p);
			} break;

			case TOK_CPAR: {
				par_cnt--;
				if (par_cnt == 0)
					goto exit;
				next(p);
			} break;

			default: {
				AST *expr = parse_expr(p, PARSE_EXPR_ARGS);
				da_append(&fc->as.func_call.args, expr);
			} break;
		}
	}


exit:
	return fc;
}

AST *parse_body(Parser *p, bool isProg);

AST *parse_func_def(Parser *p) {
	next(p);
	expect(p, TOK_ID);

	AST *fd = ast({
		.kind = AST_FUNC_DEF,
		.loc = p->lexer.cur_loc,
		.as.func_def.id = next(p).data,
	});

	expect(p, TOK_OPAR);
	next(p);

	while (peek(p).kind != TOK_CPAR) {
		switch (peek(p).kind) {
			case TOK_COM:
				if(peek2(p).kind == TOK_COM)
					lexer_error(peek(p).loc, "error: too many coms");
				break;
			case TOK_ID:
				da_append(&fd->as.func_def.args, peek(p).data);
				break;
			default: lexer_error(peek(p).loc, "error: wrong func args");
		}

		next(p);
	}

	next(p);

	symbol_stack_add(&p->stack, (Symbol){
		.kind = SYMBOL_FUNC,
		.id = fd->as.func_def.id,
		.as.func_args_cnt = fd->as.func_def.args.count,
	});

	size_t stack_cnt = p->stack.count;
	da_foreach (char*, it, &fd->as.func_def.args) {
		symbol_stack_add(&p->stack, (Symbol){
			.kind = SYMBOL_VAR,
			.id = *it,
		});
	}

	fd->as.func_def.body = parse_body(p, false);
	p->stack.count = stack_cnt;
	return fd;
}

AST *parse_expr(Parser *p, ParseExprKind pek) {
	ASTs nodes = {0};

	for (;;) {
		if (pek == PARSE_EXPR_SEMI) {
			if (peek(p).kind == TOK_SEMI) break;
		} else if (pek == PARSE_EXPR_CPAR) {
			if (peek(p).kind == TOK_CPAR) break;
		} else if (pek == PARSE_EXPR_ARGS) {
			if (peek(p).kind == TOK_COM ||
				peek(p).kind == TOK_CPAR) break;
		}

		switch (peek(p).kind) {
			case TOK_OPAR: {
				next(p);
				da_append(&nodes, parse_expr(p, PARSE_EXPR_CPAR));
			} break;

			case TOK_ID: {
				Symbol *vs = symbol_stack_get(&p->stack, peek(p));
				if (peek2(p).kind == TOK_OPAR) {
					da_append(&nodes, parse_func_call(p));
				} else {
					da_append(&nodes, ast({
						.kind = AST_VAR,
						.loc = peek(p).loc,
						.type = vs->as.var_type,
						.as.var = peek(p).data,
					}));
				}
			} break;

			case TOK_INT: {
				da_append(&nodes, ast({
					.kind = AST_LIT,
					.loc = peek(p).loc,
					.type = (Type){.kind = TYPE_INT},
					.as.lit.kind = LITERAL_INT,
					.as.lit.as.int_val = parse_int(peek(p).data),
				}));
			} break;

			case TOK_FLOAT: {
				da_append(&nodes, ast({
					.kind = AST_LIT,
					.loc = peek(p).loc,
					.type = (Type){.kind = TYPE_FLOAT},
					.as.lit.kind = LITERAL_FLOAT,
					.as.lit.as.float_val = parse_float(peek(p).data),
				}));
			} break;

			case TOK_PLUS: case TOK_MINUS:
			case TOK_STAR: case TOK_SLASH:
			case TOK_EQ: {
				TokenKind tk = peek(p).kind;
				da_append(&nodes, ast({
					.kind = AST_BIN_EXPR,
					.loc = peek(p).loc,
					.as.bin_expr.op =
						tk == TOK_EQ    ? AST_OP_EQ  :
						tk == TOK_PLUS  ? AST_OP_ADD :
						tk == TOK_MINUS ? AST_OP_SUB :
						tk == TOK_STAR  ? AST_OP_MUL :
						tk == TOK_SLASH ? AST_OP_DIV : 0,
				}));
			} break;

			default: lexer_error(peek(p).loc, "error: wrong expression");
		}

		next(p);
	}

	return parse_expand(&nodes);
}

AST *parse_func_ret(Parser *p) {
	next(p);

	AST *fr = ast({
		.kind = AST_RET,
		.as.ret.expr = parse_expr(p, PARSE_EXPR_SEMI),
	});

	return fr;
}

AST *parse_var_def_assign(Parser *p) {
	AST *vd = ast({
		.kind = AST_VAR_DEF,
		.as.var_def.id = next(p).data,
	});

	expect(p, TOK_ASSIGN);
	next(p);

	vd->as.var_def.expr = parse_expr(p, PARSE_EXPR_SEMI);
	symbol_stack_add(&p->stack, (Symbol){
		.kind = SYMBOL_VAR,
		.id = vd->as.var_def.id,
		.as.var_type = (Type){0},
	});
	return vd;
}

AST *parse_var_mut(Parser *p) {
	AST *vm = ast({
		.kind = AST_VAR_MUT,
		.as.var_mut = parse_expr(p, PARSE_EXPR_SEMI),
	});

	return vm;
}

AST *parse_body(Parser *p, bool isProg) {
	AST *body = ast({.kind = AST_BODY});
	if (!isProg) {
		expect(p, TOK_OBRA);
		next(p);
	}

	while ((!isProg && peek(p).kind != TOK_CBRA) ||
		(isProg && peek(p).kind != TOK_EOF)) {
		switch (peek(p).kind) {
			case TOK_ID: {
				if (peek2(p).kind == TOK_ASSIGN)
					da_append(&body->as.body, parse_var_def_assign(p));
				else
					da_append(&body->as.body, parse_var_mut(p));
			} break;

			case TOK_FUNC: {
				da_append(&body->as.body, parse_func_def(p));
			} break;

			case TOK_RET: {
				da_append(&body->as.body, parse_func_ret(p));
			} break;

			default: lexer_error(peek(p).loc, "error: unknown body level declaration");
		}

		next(p);
	}

	return body;
}

AST *parse(Parser *p) {
	AST *prog = ast({.kind = AST_PROG});
	prog->as.prog.body = parse_body(p, true);
	return prog;
}
