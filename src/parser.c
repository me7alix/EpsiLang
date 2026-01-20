#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include "../include/parser.h"

void parser_error(Parser *p, Location loc, char *msg) {
	p->err_ctx.got_err = true;
	p->err_ctx.errf(loc, ERROR_COMPTIME, msg);
}

AST *ast_alloc(AST ast) {
	AST *n = malloc(sizeof(AST));
	*n = ast;
	return n;
}

#define expect(p, tk) expect_f(p, tk, #tk)
void expect_f(Parser *p, TokenKind tk, char *tok_str) {
	if (peek(p).kind == TOK_ERR) {
		parser_error(p, peek(p).loc, peek(p).data);
		return;
	}

	if (peek(p).kind != tk) {
		char err[1024];
		sprintf(err, "%s expected", tok_str);
		parser_error(p, peek(p).loc, err);
	}
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
		case AST_OP_ADD_EQ:
		case AST_OP_SUB_EQ:
		case AST_OP_MUL_EQ:
		case AST_OP_DIV_EQ:
		case AST_OP_PAIR:
			return l ? 10 : 11;
		case AST_OP_AND:
		case AST_OP_OR:
			return l ? 15 : 16;
		case AST_OP_NOT_EQ:
		case AST_OP_IS_EQ:
		case AST_OP_GREAT:
		case AST_OP_GREAT_EQ:
		case AST_OP_LESS:
		case AST_OP_LESS_EQ:
			return l ? 20 : 21;
		case AST_OP_SUB:
		case AST_OP_ADD:
			return l ? 70 : 71;
		case AST_OP_MOD:
		case AST_OP_MUL:
		case AST_OP_DIV:
			return l ? 80 : 81;
		case AST_OP_ARR:
			return l ? 100 : 101;
		case AST_OP_NOT:
		case AST_OP_NEG:
			return l ? 0 : 111;
		default: return 0;
	}
}

AST_Op ast_get_op(Parser *p, AST *a) {
	switch (a->kind) {
		case AST_UN_EXPR:
			return a->as.un_expr.op;
		case AST_BIN_EXPR:
			return a->as.bin_expr.op;
		default:
			parser_error(p, a->loc, "invalid expression");
			return 0;
	}
}

typedef enum {
	PARSE_EXPR_STMT,
	PARSE_EXPR_PARS,
	PARSE_EXPR_ARGS,
	PARSE_EXPR_BODY,
	PARSE_EXPR_SQBRAS,
} ParseExprKind;

AST *parse_expr(Parser *p, ParseExprKind pek);

AST *parse_list(Parser *p) {
	AST *list = ast_alloc((AST){
		.kind = AST_LIST,
		.loc = peek(p).loc,
		.as.list = NULL,
	});

	if (peek2(p).kind == TOK_CSQBRA) {
		next(p);
		return list;
	}

	next(p);
	for (;;) {
		switch (peek(p).kind) {
			case TOK_CSQBRA: goto exit;

			case TOK_COM: {
				if(peek2(p).kind == TOK_COM) {
					parser_error(p, peek(p).loc, "too many coms");
					return NULL;
				}
				next(p);
			} break;

			default: {
				AST *expr = parse_expr(p, PARSE_EXPR_ARGS);
				if (p->err_ctx.got_err) return NULL;
				da_append(&list->as.list, expr);
				if (peek(p).kind != TOK_CSQBRA && peek(p).kind != TOK_COM) {
					parser_error(p, peek(p).loc, "invalid expression");
					return NULL;
				}
			} break;
		}
	}


exit:
	expect(p, TOK_CSQBRA);
	return list;
}

AST *parse_dict(Parser *p) {
	AST *dict = ast_alloc((AST){
		.kind = AST_DICT,
		.loc = peek(p).loc,
		.as.dict = {0},
	});

	if (peek2(p).kind == TOK_CBRA) {
		next(p);
		return dict;
	}

	next(p);
	for (;;) {
		switch (peek(p).kind) {
			case TOK_CBRA: goto exit;

			case TOK_COM: {
				if(peek2(p).kind == TOK_COM) {
					parser_error(p, peek(p).loc, "too many coms");
					return NULL;
				}
				next(p);
			} break;

			default: {
				AST *expr = parse_expr(p, PARSE_EXPR_ARGS);
				if (p->err_ctx.got_err) return NULL;

				if (expr->kind != AST_BIN_EXPR && expr->as.bin_expr.op != AST_OP_PAIR) {
					parser_error(p, expr->loc, "key-value pair expected");
					return NULL;
				}

				da_append(&dict->as.dict, expr);
				if (peek(p).kind != TOK_CBRA && peek(p).kind != TOK_COM) {
					parser_error(p, peek(p).loc, "invalid expression");
					return NULL;
				}
			} break;
		}
	}

exit:
	expect(p, TOK_CBRA);
	return dict;
}

AST *parse_func_call(Parser *p) {
	AST *func_call = ast_alloc((AST){
		.kind = AST_FUNC_CALL,
		.loc = peek(p).loc,
	});

	func_call->as.func_call.id = next(p).data;
	expect(p, TOK_OPAR); next(p);

	for (;;) {
		switch (peek(p).kind) {
			case TOK_CPAR: goto exit;

			case TOK_COM: {
				if(peek2(p).kind == TOK_COM) {
					parser_error(p, peek(p).loc, "too many coms");
					return NULL;
				}
				next(p);
			} break;

			default: {
				AST *expr = parse_expr(p, PARSE_EXPR_ARGS);
				if (p->err_ctx.got_err) return NULL;
				da_append(&func_call->as.func_call.args, expr);

				if (peek(p).kind != TOK_CPAR && peek(p).kind != TOK_COM) {
					parser_error(p, peek(p).loc, "invalid expression");
					return NULL;
				}
			} break;
		}
	}

exit:
	return func_call;
}

AST *parse_body(Parser *p, bool isProg);

AST *parse_if_stmt(Parser *p) {
	AST *if_st = ast_alloc((AST){
		.kind = AST_ST_IF,
		.loc = next(p).loc,
	});

	if_st->as.st_if_chain.cond = parse_expr(p, PARSE_EXPR_BODY);
	if (p->err_ctx.got_err) return NULL;
	if_st->as.st_if_chain.body = parse_body(p, false);
	if (p->err_ctx.got_err) return NULL;

	if (peek2(p).kind == TOK_ELSE_SYM) {
		next(p);

		if (peek2(p).kind == TOK_IF_SYM) {
			next(p);
			if_st->as.st_if_chain.chain = parse_if_stmt(p);
			if (p->err_ctx.got_err) return NULL;
			return if_st;
		}

		AST *elst = ast_alloc((AST){
			.kind = AST_ST_ELSE,
			.loc = next(p).loc
		});

		elst->as.st_else.body = parse_body(p, false);
		if (p->err_ctx.got_err) return NULL;
		if_st->as.st_if_chain.chain = elst;
	}

	return if_st;
}

AST *parse_var_mut(Parser *p, ParseExprKind pek) {
	AST *var_mut = ast_alloc((AST){
		.kind = AST_VAR_MUT,
		.loc = peek(p).loc,
		.as.var_mut = parse_expr(p, pek),
	});

	return var_mut;
}

AST *parse_var_def_assign(Parser *p) {
	AST *var_def = ast_alloc((AST){
		.kind = AST_VAR_DEF,
		.loc = peek(p).loc,
		.as.var_def.id = next(p).data,
	});

	expect(p, TOK_ASSIGN);
	next(p);

	var_def->as.var_def.expr = parse_expr(p, PARSE_EXPR_STMT);
	if (p->err_ctx.got_err) return NULL;
	return var_def;
}

AST *parse_for_stmt(Parser *p) {
	AST *for_st = ast_alloc((AST){
		.loc = next(p).loc,
	});

	expect(p, TOK_ID);
	if (p->err_ctx.got_err) return NULL;

	if (peek2(p).kind != TOK_ID && strcmp(peek2(p).data, "in") != 0) {
		for_st->kind = AST_ST_FOR;
		if (peek2(p).kind == TOK_ASSIGN)
			for_st->as.st_for.var = parse_var_def_assign(p);
		else
			for_st->as.st_for.var = parse_var_mut(p, PARSE_EXPR_STMT);
		if (p->err_ctx.got_err) return NULL;
		next(p);

		for_st->as.st_for.cond = parse_expr(p, PARSE_EXPR_STMT);
		if (p->err_ctx.got_err) return NULL;
		next(p);

		for_st->as.st_for.mut = parse_var_mut(p, PARSE_EXPR_BODY);
		if (p->err_ctx.got_err) return NULL;
	} else {
		for_st->kind = AST_ST_FOREACH;
		for_st->as.st_foreach.var_id = next(p).data; next(p);
		for_st->as.st_foreach.coll = parse_expr(p, PARSE_EXPR_BODY);
		if (p->err_ctx.got_err) return NULL;
	}


	for_st->as.st_for.body = parse_body(p, false);
	return for_st;
}

AST *parse_while_stmt(Parser *p) {
	AST *wst = ast_alloc((AST){
		.kind = AST_ST_WHILE,
		.loc = next(p).loc,
	});

	wst->as.st_while.cond = parse_expr(p, PARSE_EXPR_BODY);
	if (p->err_ctx.got_err) return NULL;
	wst->as.st_while.body = parse_body(p, false);
	if (p->err_ctx.got_err) return NULL;
	return wst;
}

AST *parse_func_def(Parser *p) {
	next(p); expect(p, TOK_ID);
	if (p->err_ctx.got_err) return NULL;

	AST *func_def = ast_alloc((AST){
		.kind = AST_FUNC_DEF,
		.loc = peek(p).loc,
	});

	func_def->as.func_def.id = next(p).data;
	expect(p, TOK_OPAR);
	if (p->err_ctx.got_err) return NULL;
	next(p);

	bool found_any = false;
	while (peek(p).kind != TOK_CPAR) {
		switch (peek(p).kind) {
			case TOK_COM: {
				if(peek2(p).kind == TOK_COM) {
					parser_error(p, peek(p).loc, "too many coms");
					return NULL;
				}
			} break;

			case TOK_ANY: {
				if (found_any) {
					parser_error(p, peek(p).loc, "only one variadic parameter is allowed");
					return NULL;
				}

				found_any = true;
				da_append(&func_def->as.func_def.args, ast_alloc((AST){
					.kind = AST_VAR_ANY,
					.loc = peek(p).loc,
				}));
			} break;

			case TOK_ID: {
				da_append(&func_def->as.func_def.args, ast_alloc((AST){
					.kind = AST_VAR,
					.loc = peek(p).loc,
					.as.var = peek(p).data,
				}));
			} break;

			default:
				parser_error(p, peek(p).loc, "invalid function argument");
				return NULL;
		}

		next(p);
	}

	next(p);

	func_def->as.func_def.body = parse_body(p, false);
	return func_def;
}

void ast_expr_set_val(AST *expr, AST *n, bool left) {
	switch (expr->kind) {
		case AST_BIN_EXPR: {
			if (left) expr->as.bin_expr.lhs = n;
			else      expr->as.bin_expr.rhs = n;
		} break;

		case AST_UN_EXPR: {
			expr->as.un_expr.v = n;
		} break;

		default: assert(0);
	}
}

AST *parse_expand(Parser *p, ASTs *nodes) {
	if (p->err_ctx.got_err) return NULL;
	size_t cnt_before = nodes->count;

	for (size_t i = 0; i < nodes->count; i++) {
		if (nodes->count == 1)
			return da_last(nodes);

		AST *node = da_get(nodes, i);
		bool is_op = false;
		if (node->kind == AST_BIN_EXPR) {
			if (!node->as.bin_expr.lhs || !node->as.bin_expr.rhs)
				is_op = true;
		} else if (node->kind == AST_UN_EXPR) {
			if (!node->as.un_expr.v)
				is_op = true;
		}

		if (!is_op) {
			uint lpr = 0, rpr = 0;

			if (i > 0)
				lpr = ast_op_precedence(ast_get_op(p, da_get(nodes, i - 1)), false);
			if (p->err_ctx.got_err) return NULL;

			if (i < nodes->count - 1)
				rpr = ast_op_precedence(ast_get_op(p, da_get(nodes, i + 1)), true);
			if (p->err_ctx.got_err) return NULL;

			if (lpr == 0 && rpr == 0) {
				parser_error(p, node->loc, "invalid combination of operator and operands");
				return NULL;
			}

			if (lpr > rpr) ast_expr_set_val(da_get(nodes, i - 1), node, false);
			else           ast_expr_set_val(da_get(nodes, i + 1), node, true);

			da_remove_ordered(nodes, i);
			i--;
		}
	}

	if (cnt_before == nodes->count) {
		parser_error(p, da_last(nodes)->loc, "invalid expression");
		return NULL;
	}

	return parse_expand(p, nodes);
}

AST *parse_expr(Parser *p, ParseExprKind pek) {
	ASTs nodes = {0};

	for (;;) {
		if (p->err_ctx.got_err) return NULL;
		if (pek == PARSE_EXPR_STMT) {
			if (peek(p).kind == TOK_SEMI) break;
		} else if (pek == PARSE_EXPR_PARS) {
			if (peek(p).kind == TOK_CPAR) break;
		} else if (pek == PARSE_EXPR_ARGS) {
			if (peek(p).kind == TOK_COM ||
				peek(p).kind == TOK_CSQBRA ||
				peek(p).kind == TOK_CBRA ||
				peek(p).kind == TOK_CPAR) break;
		} else if (pek == PARSE_EXPR_BODY) {
			if (peek(p).kind == TOK_ARROW_EQ ||
				peek(p).kind == TOK_ARROW ||
				peek(p).kind == TOK_OBRA) break;
		} else if (pek == PARSE_EXPR_SQBRAS) {
			if (peek(p).kind == TOK_CSQBRA) break;
		}

		switch (peek(p).kind) {
			case TOK_OSQBRA: {
				if (nodes.count > 0) {
					if (da_last(&nodes)->kind == AST_VAR) {
						da_append(&nodes, ast_alloc((AST){
							.kind = AST_BIN_EXPR,
							.loc = next(p).loc,
							.as.bin_expr.op = AST_OP_ARR,
						}));

						da_append(&nodes, parse_expr(p, PARSE_EXPR_SQBRAS));
						if (p->err_ctx.got_err) return NULL;
						break;
					}
				}

				da_append(&nodes, parse_list(p));
			} break;

			case TOK_STRING: {
				da_append(&nodes, ast_alloc((AST){
					.kind = AST_LIT,
					.loc = peek(p).loc,
					.as.lit.kind = LITERAL_STR,
					.as.lit.as.vstr = peek(p).data,
				}));
			} break;

			case TOK_NONE: {
				da_append(&nodes, ast_alloc((AST){
					.kind = AST_VAL_NONE,
					.loc = peek(p).loc,
				}));
			} break;

			case TOK_FALSE:
			case TOK_TRUE: {
				da_append(&nodes, ast_alloc((AST){
					.kind = AST_LIT,
					.loc = peek(p).loc,
					.as.lit.kind = LITERAL_BOOL,
					.as.lit.as.vbool = peek(p).kind == TOK_TRUE,
				}));
			} break;

			case TOK_OBRA: {
				da_append(&nodes, parse_dict(p));
			} break;

			case TOK_OPAR: {
				next(p);
				da_append(&nodes, parse_expr(p, PARSE_EXPR_PARS));
			} break;

			case TOK_ID: {
				if (peek2(p).kind == TOK_OPAR) {
					da_append(&nodes, parse_func_call(p));
				} else {
					da_append(&nodes, ast_alloc((AST){
						.kind = AST_VAR,
						.loc = peek(p).loc,
						.as.var = peek(p).data,
					}));
				}
			} break;

			case TOK_INT: {
				da_append(&nodes, ast_alloc((AST){
					.kind = AST_LIT,
					.loc = peek(p).loc,
					.as.lit.kind = LITERAL_INT,
					.as.lit.as.vint = parse_int(peek(p).data),
				}));
			} break;

			case TOK_FLOAT: {
				da_append(&nodes, ast_alloc((AST){
					.kind = AST_LIT,
					.loc = peek(p).loc,
					.as.lit.kind = LITERAL_FLOAT,
					.as.lit.as.vfloat = parse_float(peek(p).data),
				}));
			} break;

			case TOK_EXC: {
				da_append(&nodes, ast_alloc((AST){
					.kind = AST_UN_EXPR,
					.loc = peek(p).loc,
					.as.un_expr.op = AST_OP_NOT,
				}));
			} break;

			case TOK_MINUS: {
				bool is_unary_op = false;
				if (nodes.count == 0) {
					is_unary_op = true;
				} else {
					if (da_last(&nodes)->kind == AST_UN_EXPR) {
						is_unary_op = false;
					} else {
						bool is_bin_op = da_last(&nodes)->kind == AST_BIN_EXPR;
						if (is_bin_op && da_last(&nodes)->as.bin_expr.lhs &&
							da_last(&nodes)->as.bin_expr.rhs) is_bin_op = false;
						if (is_bin_op) is_unary_op = true;
					}
				}

				if (!is_unary_op) {
					da_append(&nodes, ast_alloc((AST){
						.kind = AST_BIN_EXPR,
						.loc = peek(p).loc,
						.as.bin_expr.op = AST_OP_SUB,
					}));
				} else {
					da_append(&nodes, ast_alloc((AST){
						.kind = AST_UN_EXPR,
						.loc = peek(p).loc,
						.as.un_expr.op = AST_OP_NEG,
					}));
				}
			} break;

			case TOK_EQ:      case TOK_OR:
			case TOK_COL:     case TOK_PS:
			case TOK_EQ_EQ:   case TOK_AND:
			case TOK_PLUS:    case TOK_STAR:
			case TOK_LESS:    case TOK_LESS_EQ:
			case TOK_GREAT:   case TOK_GREAT_EQ:
			case TOK_PLUS_EQ: case TOK_MINUS_EQ:
			case TOK_STAR_EQ: case TOK_SLASH_EQ:
			case TOK_NOT_EQ:  case TOK_SLASH: {
				TokenKind tk = peek(p).kind;
				da_append(&nodes, ast_alloc((AST){
					.kind = AST_BIN_EXPR,
					.loc = peek(p).loc,
					.as.bin_expr.op =
					tk == TOK_NOT_EQ   ? AST_OP_NOT_EQ   :
					tk == TOK_LESS     ? AST_OP_LESS     :
					tk == TOK_LESS_EQ  ? AST_OP_LESS_EQ  :
					tk == TOK_GREAT    ? AST_OP_GREAT    :
					tk == TOK_GREAT_EQ ? AST_OP_GREAT_EQ :
					tk == TOK_COL      ? AST_OP_PAIR     :
					tk == TOK_EQ_EQ    ? AST_OP_IS_EQ    :
					tk == TOK_AND      ? AST_OP_AND      :
					tk == TOK_OR       ? AST_OP_OR       :
					tk == TOK_EQ       ? AST_OP_EQ       :
					tk == TOK_PLUS     ? AST_OP_ADD      :
					tk == TOK_STAR     ? AST_OP_MUL      :
					tk == TOK_SLASH    ? AST_OP_DIV      :
					tk == TOK_PS       ? AST_OP_MOD      :
					tk == TOK_PLUS_EQ  ? AST_OP_ADD_EQ   :
					tk == TOK_MINUS_EQ ? AST_OP_SUB_EQ   :
					tk == TOK_STAR_EQ  ? AST_OP_MUL_EQ   :
					tk == TOK_SLASH_EQ ? AST_OP_DIV_EQ   : 0,
				}));
			} break;

			default: {
				if (peek(p).kind == TOK_ERR) {
					parser_error(p, peek(p).loc, peek(p).data);
					return NULL;
				}

				parser_error(p, peek(p).loc, "invalid expression");
				return NULL;
			}
		}

		next(p);
	}

	if (nodes.count == 0)
		return NULL;

	return parse_expand(p, &nodes);
}

AST *parse_func_ret(Parser *p) {
	AST *func = ast_alloc((AST){
		.kind = AST_RET,
		.loc = next(p).loc,
	});

	func->as.ret.expr = parse_expr(p, PARSE_EXPR_STMT);
	return func;
}

AST *parse_body(Parser *p, bool isProg) {
	bool is_arrow    = false;
	bool is_arrow_eq = false;

	AST *body = ast_alloc((AST){
		.kind = AST_BODY,
		.loc = peek(p).loc,
	});

	if (!isProg) {
		if (!(peek(p).kind == TOK_OBRA ||
			peek(p).kind == TOK_ARROW ||
			peek(p).kind == TOK_ARROW_EQ)) {
			parser_error(p, peek(p).loc, "invalid body declaration");
			return NULL;
		}

		if (peek(p).kind == TOK_ARROW)
			is_arrow = true;
		else if (peek(p).kind == TOK_ARROW_EQ)
			is_arrow_eq = true;

		next(p);
	}

	while ((!isProg && peek(p).kind != TOK_CBRA) ||
		(isProg && peek(p).kind != TOK_EOF)) {
		if (p->err_ctx.got_err) return NULL;
		switch (peek(p).kind) {
			case TOK_IMPORT: {
				next(p);
				expect(p, TOK_STRING);
				Parser ip = {
					.lexer = lexer_from_file(peek(p).data),
					.err_ctx = p->err_ctx,
				};

				if (!ip.lexer.cur_char) {
					parser_error(p, peek(p).loc, "no such file");
					p->err_ctx.got_err = true;
					return NULL;
				}

				AST *ib = parse_body(&ip, true);
				if (ip.err_ctx.got_err) {
					p->err_ctx.got_err = true;
					return NULL;
				}

				da_foreach (AST*, it, &ib->as.body) {
					da_append(&body->as.body, *it);
					if (ip.err_ctx.got_err) {
						p->err_ctx.got_err = true;
						return NULL;
					}
				}

				next(p);
			} break;

			case TOK_ID: {
				if (peek2(p).kind == TOK_ASSIGN)
					da_append(&body->as.body, parse_var_def_assign(p));
				else
					da_append(&body->as.body, parse_var_mut(p, PARSE_EXPR_STMT));
			} break;

			case TOK_FUNC: {
				da_append(&body->as.body, parse_func_def(p));
			} break;

			case TOK_RET: {
				da_append(&body->as.body, parse_func_ret(p));
			} break;

			case TOK_IF_SYM: {
				da_append(&body->as.body, parse_if_stmt(p));
			} break;

			case TOK_WHILE_SYM: {
				da_append(&body->as.body, parse_while_stmt(p));
			} break;

			case TOK_FOR_SYM: {
				da_append(&body->as.body, parse_for_stmt(p));
			} break;

			case TOK_BREAK: {
				da_append(&body->as.body, ast_alloc((AST){
					.kind = AST_BREAK,
					.loc = next(p).loc
				}));
			} break;

			case TOK_CONTINUE: {
				da_append(&body->as.body, ast_alloc((AST){
					.kind = AST_CONT,
					.loc = next(p).loc
				}));
			} break;

			default: {
				if (peek(p).kind == TOK_ERR) {
					parser_error(p, peek(p).loc, peek(p).data);
					return NULL;
				}

				da_append(&body->as.body, parse_var_mut(p, PARSE_EXPR_STMT));
				if(p->err_ctx.got_err) return NULL;
			}
		}

		if (is_arrow) break;
		if (is_arrow_eq) {
			AST *var_mut = da_last(&body->as.body);
			if (var_mut->kind != AST_VAR_MUT) {
				parser_error(p, var_mut->loc, "expression expected");
				return NULL;
			}

			body->as.body.count = 0;
			da_append(&body->as.body, ast_alloc((AST){
				.kind = AST_RET,
				.loc = body->loc,
				.as.ret = var_mut->as.var_mut,
			}));
			break;
		}

		next(p);
	}

	return body;
}

AST *parse(Parser *p) {
	AST *prog = ast_alloc((AST){.kind = AST_PROG});
	prog->as.prog.body = parse_body(p, true);
	return prog;
}
