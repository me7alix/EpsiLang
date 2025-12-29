#include <stdbool.h>
#include "../include/parser.h"

void expect(Parser *p, TokenKind tk) {
	if (lexer_peek(p->lexer).kind != tk)
		lexer_error(lexer_peek(p->lexer).loc, "error: unexpected token");
}

void symbol_stack_add(SymbolStack *ss, Symbol s) {
	da_append(ss, s);
}

Symbol symbol_stack_get(SymbolStack *ss, Token var) {
	for (size_t i = ss->count - 1; i >= 0; i--) {
		if (strcmp(da_get(ss, i).id, var.data) == 0) {
			return da_get(ss, i);
		}
	}

	lexer_error(var.loc, "error: no such symbol at the scope");
	return (Symbol){0};
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
	}
}

AST *parse_expand(ASTs *nodes) {
	if (nodes->count == 1)
		return da_get(nodes, 0);

	for (size_t i = 0; i < nodes->count; i++) {
		AST *node = da_get(nodes, i);

		if (!(node->kind == AST_BIN_EXPR && (!node->as.bin_expr.lhs || !node->as.bin_expr.rhs))) {
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

#define asts_append(asts, ast) \
	do { \
		AST *new = malloc(sizeof(AST)); \
		*new = ast; \
		da_append(asts, new); \
	} while (0)

AST *parse_expr(Parser *p) {
	ASTs nodes = {0};

	while (lexer_peek(p->lexer).kind != TOK_SEMI) {
		switch (lexer_peek(p->lexer).kind) {
			case TOK_ID: {
				asts_append(&nodes, ((AST){
					.kind = AST_VAR,
					.loc = lexer_peek(p->lexer).loc,
					.as.var = lexer_peek(p->lexer).data,
				}));
			} break;

			case TOK_INT: {
				asts_append(&nodes, ((AST){
					.kind = AST_LIT,
					.loc = lexer_peek(p->lexer).loc,
					.as.lit.kind = LITERAL_INT,
					.as.lit.as.int_val = parse_int(lexer_peek(p->lexer).data),
				}));
			} break;

			case TOK_FLOAT: {
				asts_append(&nodes, ((AST){
					.kind = AST_LIT,
					.type = (Type){
						.kind = TYPE_INT,
						.as.tfloat.size = 4
					},
					.loc = lexer_peek(p->lexer).loc,
					.as.lit.kind = LITERAL_FLOAT,
					.as.lit.as.float_val = parse_float(lexer_peek(p->lexer).data),
				}));
			} break;

			case TOK_EQ:
			case TOK_PLUS: case TOK_MINUS:
			case TOK_STAR: case TOK_SLASH: {
				TokenKind tk = lexer_peek(p->lexer).kind;
				asts_append(&nodes, ((AST){
					.kind = AST_BIN_EXPR,
					.loc = lexer_peek(p->lexer).loc,
					.as.bin_expr.op =
						tk == TOK_EQ    ? AST_OP_EQ  :
						tk == TOK_PLUS  ? AST_OP_ADD :
						tk == TOK_MINUS ? AST_OP_SUB :
						tk == TOK_STAR  ? AST_OP_MUL :
						tk == TOK_SLASH ? AST_OP_DIV : 0,
					.as.bin_expr.lhs = NULL,
					.as.bin_expr.rhs = NULL,
				}));
			} break;

			default: lexer_error(p->lexer->cur_loc, "error: wrong expression");
		}

		lexer_next(p->lexer);
	}

	return parse_expand(&nodes);
}

AST *parse_var_def_assign(Parser *p) {
	AST *vd = malloc(sizeof(AST));
	*vd = (AST){
		.kind = AST_VAR_DEF,
		.as.var_def.id = lexer_next(p->lexer).data,
	};

	expect(p, TOK_ASSIGN);
	lexer_next(p->lexer);

	vd->as.var_def.expr = parse_expr(p);
	return vd;
}

AST *parse_var_mut(Parser *p) {
	AST *vm = malloc(sizeof(AST));
	*vm = (AST){
		.kind = AST_VAR_MUT,
		.as.var_mut = parse_expr(p),
	};

	return vm;
}

AST *parse_body(Parser *p, bool isProg) {
	AST *body = malloc(sizeof(AST));
	*body = (AST){.kind = AST_BODY};
	
	if (!isProg) {
		lexer_next(p->lexer);
		expect(p, TOK_OBRA);
		lexer_next(p->lexer);
	}

	while ((!isProg && lexer_peek(p->lexer).kind != TOK_CBRA) ||
		    (isProg && lexer_peek(p->lexer).kind != TOK_EOF)) {
		switch (lexer_peek(p->lexer).kind) {
			case TOK_ID: {
				if (lexer_peek2(p->lexer).kind == TOK_ASSIGN) {
					da_append(&body->as.body, parse_var_def_assign(p));
				} else {
					da_append(&body->as.body, parse_var_mut(p));
				}
			} break;

			default: lexer_error(p->lexer->cur_loc, "error: unknown body declaration");
		}

		lexer_next(p->lexer);
	}

	return body;
}

AST *parser_parse(Lexer *lexer) {
	Parser p = {lexer};

	AST *prog = malloc(sizeof(AST));
	*prog = (AST){.kind = AST_PROG};

	prog->as.prog.body = parse_body(&p, true);

	return prog;
}
