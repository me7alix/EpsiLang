#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include "../include/parser.h"

#define peek(p)  lexer_peek(&(p)->lexer)
#define next(p)  lexer_next(&(p)->lexer)
#define peek2(p) lexer_peek2(&(p)->lexer)

void parser_error(Parser *p, Location loc, char *msg) {
	p->err_ctx.got_err = true;
	p->err_ctx.errf(NULL, loc, ERROR_COMPTIME, msg);
}

static size_t stack_ptrs[256];
static size_t stack_ptr = 0;
static uint vid = 1;

void symbol_table_push(Parser *p) {
	stack_ptrs[stack_ptr++] = p->symbols.count;
}

void symbol_table_pop(Parser *p) {
	p->symbols.count = stack_ptrs[--stack_ptr];
}

AST_Var symbol_table_get(Parser *p, int kind, char *id) {
	for (int i = (int)(p->symbols.count) - 1; i >= 0; i--) {
		AST_Symbol sbl = p->symbols.items[i];
		if (strcmp(id, sbl.var.id) == 0 && sbl.kind == kind) {
			return p->symbols.items[i].var;
		}
	}

	return (AST_Var){id, 0, 0};
}

AST_Var symbol_table_add(Parser *p, Location loc, int kind, char *id) {
	for (size_t i = stack_ptrs[stack_ptr-1]; i < p->symbols.count; i++) {
		AST_Symbol var = p->symbols.items[i];
		if (var.kind == kind && strcmp(var.var.id, id) == 0) {
			parser_error(p, loc, "redefinition of a variable");
			return (AST_Var){0};
		}
	}

	size_t stack_base = stack_ptrs[stack_ptr - 1];
	uint idx = (uint)(p->symbols.count - stack_base);
	AST_Var var = {id, vid++, idx};
	da_append(&p->symbols, ((AST_Symbol){kind, var}));
	return var;
}

#define ast(p, ...) ast_alloc(p, (AST){__VA_ARGS__})
AST *ast_alloc(Parser *p, AST ast) {
	AST *n = arena_alloc(&p->arena, sizeof(AST));
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

uint op_precedence(AST_Op op, bool l) {
	switch (op) {
	case AST_OP_EQ:
	case AST_OP_ADD_EQ:
	case AST_OP_SUB_EQ:
	case AST_OP_MUL_EQ:
	case AST_OP_DIV_EQ:
	case AST_OP_PAIR:
		return l ? 10 : 11;
	case AST_OP_OR:
		return l ? 15 : 16;
	case AST_OP_AND:
		return l ? 17 : 18;
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
	case AST_OP_FIELD:
	case AST_OP_NOT:
	case AST_OP_NEG:
		return l ? 0 : 111;
	case AST_OP_ARR:
	case AST_OP_GET_FIELD:
		return l ? 110 : 111;
	default: return 0;
	}
}

AST_Op get_op(Parser *p, AST *a) {
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

AST *expr(Parser *p, ParseExprKind pek);

AST *list(Parser *p) {
	AST *list = ast(p,
		.kind = AST_LIST,
		.loc = peek(p).loc,
		.as.list = {.arena = &p->arena});

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
				AST *ex = expr(p, PARSE_EXPR_ARGS);
				if (p->err_ctx.got_err) return NULL;
				da_append(&list->as.list, ex);
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

AST *dict(Parser *p) {
	AST *dict = ast(p,
		.kind = AST_DICT,
		.loc = peek(p).loc,
		.as.dict = {.arena = &p->arena});

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
				AST *ex = expr(p, PARSE_EXPR_ARGS);
				if (p->err_ctx.got_err) return NULL;
				if (ex->kind != AST_BIN_EXPR || ex->as.bin_expr.op != AST_OP_PAIR) {
					parser_error(p, ex->loc, "key-value pair expected");
					return NULL;
				}
				da_append(&dict->as.dict, ex);
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

AST *func_call(Parser *p) {
	AST *func_call = ast(p,
		.kind = AST_FUNC_CALL,
		.loc = peek(p).loc,
		.as.func_call.args = {.arena = &p->arena});

	char *ident = next(p).data;
	func_call->as.func_call.var = symbol_table_get(p, AST_SBL_FUNC, ident);
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
				AST *ex = expr(p, PARSE_EXPR_ARGS);
				if (p->err_ctx.got_err) return NULL;
				da_append(&func_call->as.func_call.args, ex);

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

AST *scope_body(Parser *p, bool, bool);

AST *if_stmt(Parser *p) {
	AST *if_st = ast(p,
		.kind = AST_ST_IF,
		.loc = next(p).loc,
	);

	if_st->as.st_if_chain.cond = expr(p, PARSE_EXPR_BODY);
	if (p->err_ctx.got_err) return NULL;
	if_st->as.st_if_chain.body = scope_body(p, false, false);
	if (p->err_ctx.got_err) return NULL;

	if (peek2(p).kind == TOK_ELSE_SYM) {
		next(p);

		if (peek2(p).kind == TOK_IF_SYM) {
			next(p);
			if_st->as.st_if_chain.chain = if_stmt(p);
			if (p->err_ctx.got_err) return NULL;
			return if_st;
		}

		AST *elst = ast(p,
			.kind = AST_ST_ELSE,
			.loc = next(p).loc
		);

		elst->as.st_else.body = scope_body(p, false, false);
		if (p->err_ctx.got_err) return NULL;
		if_st->as.st_if_chain.chain = elst;
	}

	return if_st;
}

AST *var_mut(Parser *p, ParseExprKind pek) {
	AST *var_mut = ast(p,
		.kind = AST_VAR_MUT,
		.loc = peek(p).loc,
		.as.var_mut = expr(p, pek),
	);

	return var_mut;
}

AST *var_def_assign(Parser *p) {
	AST *var_def = ast(p,
		.kind = AST_VAR_DEF,
		.loc = peek(p).loc,
	);

	char *ident = next(p).data;
	expect(p, TOK_ASSIGN);
	next(p);

	var_def->as.var_def.var = symbol_table_add(p, var_def->loc, AST_SBL_VAR, ident);
	if (p->err_ctx.got_err) return NULL;
	var_def->as.var_def.expr = expr(p, PARSE_EXPR_STMT);
	if (p->err_ctx.got_err) return NULL;
	return var_def;
}

AST *for_stmt(Parser *p) {
	AST *for_st = ast(p, .loc = next(p).loc);

	expect(p, TOK_ID);
	if (p->err_ctx.got_err) return NULL;

	symbol_table_push(p);

	if (peek2(p).kind != TOK_IN) {
		for_st->kind = AST_ST_FOR;
		if (peek2(p).kind == TOK_ASSIGN)
			for_st->as.st_for.var = var_def_assign(p);
		else
			for_st->as.st_for.var = var_mut(p, PARSE_EXPR_STMT);
		if (p->err_ctx.got_err) return NULL;
		next(p);

		for_st->as.st_for.cond = expr(p, PARSE_EXPR_STMT);
		if (p->err_ctx.got_err) return NULL;
		next(p);

		for_st->as.st_for.mut = var_mut(p, PARSE_EXPR_BODY);
		if (p->err_ctx.got_err) return NULL;
	} else {
		for_st->kind = AST_ST_FOREACH;
		Location loc = peek(p).loc;
		char *id = next(p).data; next(p);
		for_st->as.st_foreach.var = symbol_table_add(p, loc, AST_SBL_VAR, id);
		if (p->err_ctx.got_err) return NULL;
		for_st->as.st_foreach.coll = expr(p, PARSE_EXPR_BODY);
		if (p->err_ctx.got_err) return NULL;
	}


	for_st->as.st_for.body = scope_body(p, false, false);

	symbol_table_pop(p);
	return for_st;
}

AST *while_stmt(Parser *p) {
	AST *wst = ast(p,
		.kind = AST_ST_WHILE,
		.loc = next(p).loc,
	);

	wst->as.st_while.cond = expr(p, PARSE_EXPR_BODY);
	if (p->err_ctx.got_err) return NULL;
	wst->as.st_while.body = scope_body(p, false, false);
	if (p->err_ctx.got_err) return NULL;
	return wst;
}

AST *func_def(Parser *p) {
	next(p); expect(p, TOK_ID);
	if (p->err_ctx.got_err) return NULL;

	AST *func_def = ast(p,
		.kind = AST_FUNC_DEF,
		.loc = peek(p).loc,
		.as.func_def.args = {.arena = &p->arena});

	func_def->as.func_def.var = symbol_table_add(p, func_def->loc, AST_SBL_FUNC, next(p).data);
	if (p->err_ctx.got_err) return NULL;

	expect(p, TOK_OPAR);
	if (p->err_ctx.got_err) return NULL;
	next(p);

	symbol_table_push(p);
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
					parser_error(p, peek(p).loc, "only one variadic argument is allowed");
					return NULL;
				}

				found_any = true;
				AST_Var var = symbol_table_add(p, peek(p).loc, AST_SBL_VAR, "_VA_ARGS_");
				if (p->err_ctx.got_err) return NULL;
				da_append(&func_def->as.func_def.args, ast(p,
					.kind = AST_VAR_ANY,
					.loc = peek(p).loc,
					.as.var = var,
				));
			} break;

			case TOK_ID: {
				AST_Var var = symbol_table_add(p, peek(p).loc, AST_SBL_VAR, peek(p).data);
				if (p->err_ctx.got_err) return NULL;
				da_append(&func_def->as.func_def.args, ast(p,
					.kind = AST_VAR,
					.loc = peek(p).loc,
					.as.var = var,
				));
			} break;

			default: {
				parser_error(p, peek(p).loc, "invalid function argument");
				return NULL;
			}
		}

		next(p);
	}

	next(p);

	func_def->as.func_def.body = scope_body(p, false, true);
	symbol_table_pop(p);
	return func_def;
}

void set_val(AST *ex, AST *n, bool left) {
	switch (ex->kind) {
		case AST_BIN_EXPR: {
			if (left) ex->as.bin_expr.lhs = n;
			else      ex->as.bin_expr.rhs = n;
		} break;

		case AST_UN_EXPR: {
			ex->as.un_expr.v = n;
		} break;

		default: assert(0);
	}
}

AST *expand(Parser *p, ASTs *nodes) {
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
				lpr = op_precedence(get_op(p, da_get(nodes, i - 1)), false);
			if (p->err_ctx.got_err) return NULL;

			if (i < nodes->count - 1)
				rpr = op_precedence(get_op(p, da_get(nodes, i + 1)), true);
			if (p->err_ctx.got_err) return NULL;

			if (lpr == 0 && rpr == 0) {
				parser_error(p, node->loc, "invalid combination of operator and operands");
				return NULL;
			}

			if (lpr > rpr) set_val(da_get(nodes, i - 1), node, false);
			else           set_val(da_get(nodes, i + 1), node, true);

			da_remove_ordered(nodes, i);
			i--;
		}
	}

	if (cnt_before == nodes->count) {
		parser_error(p, da_last(nodes)->loc, "invalid expression");
		return NULL;
	}

	return expand(p, nodes);
}

AST *expr(Parser *p, ParseExprKind pek) {
	ASTs nodes = {.arena = &p->arena};

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
					AST *l = da_last(&nodes);
					AST *ll = da_get(&nodes, nodes.count - 2);

					bool is_var = l->kind == AST_VAR;
					bool is_arr_op = nodes.count < 2 ? false :
						ll->kind == AST_BIN_EXPR && ll->as.bin_expr.op == AST_OP_ARR;

					if (is_arr_op || is_var) {
						da_append(&nodes, ast(p,
							.kind = AST_BIN_EXPR,
							.loc = next(p).loc,
							.as.bin_expr.op = AST_OP_ARR,
						));

						da_append(&nodes, expr(p, PARSE_EXPR_SQBRAS));
						if (p->err_ctx.got_err) return NULL;
						break;
					}
				}

				da_append(&nodes, list(p));
			} break;

			case TOK_STRING: {
				da_append(&nodes, ast(p,
					.kind = AST_LIT,
					.loc = peek(p).loc,
					.as.lit.kind = LITERAL_STR,
					.as.lit.as.vstr = peek(p).data,
				));
			} break;

			case TOK_CHAR: {
				UTF8_Rune rune = utf8_get_nth(peek(p).data, 0);
				da_append(&nodes, ast(p,
					.kind = AST_LIT,
					.loc = peek(p).loc,
					.as.lit.kind = LITERAL_RUNE,
					.as.lit.as.vrune = rune,
				));
			} break;

			case TOK_NONE: {
				da_append(&nodes, ast(p,
					.kind = AST_VAL_NONE,
					.loc = peek(p).loc,
				));
			} break;

			case TOK_FALSE:
			case TOK_TRUE: {
				da_append(&nodes, ast(p,
					.kind = AST_LIT,
					.loc = peek(p).loc,
					.as.lit.kind = LITERAL_BOOL,
					.as.lit.as.vbool = peek(p).kind == TOK_TRUE,
				));
			} break;

			case TOK_OBRA: {
				da_append(&nodes, dict(p));
			} break;

			case TOK_OPAR: {
				next(p);
				da_append(&nodes, expr(p, PARSE_EXPR_PARS));
			} break;

			case TOK_ID: {
				if (peek2(p).kind == TOK_OPAR) {
					da_append(&nodes, func_call(p));
				} else {
					da_append(&nodes, ast(p,
						.kind = AST_VAR,
						.loc = peek(p).loc,
						.as.var = symbol_table_get(p, AST_SBL_VAR, peek(p).data)
					));
				}
			} break;

			case TOK_INT: {
				da_append(&nodes, ast(p,
					.kind = AST_LIT,
					.loc = peek(p).loc,
					.as.lit.kind = LITERAL_INT,
					.as.lit.as.vint = parse_int(peek(p).data),
				));
			} break;

			case TOK_FLOAT: {
				da_append(&nodes, ast(p,
					.kind = AST_LIT,
					.loc = peek(p).loc,
					.as.lit.kind = LITERAL_FLOAT,
					.as.lit.as.vfloat = parse_float(peek(p).data),
				));
			} break;

			case TOK_EXC: {
				da_append(&nodes, ast(p,
					.kind = AST_UN_EXPR,
					.loc = peek(p).loc,
					.as.un_expr.op = AST_OP_NOT,
				));
			} break;

			case TOK_DOT:
			case TOK_MINUS: {
				int op = peek(p).kind;
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
					da_append(&nodes, ast(p,
						.kind = AST_BIN_EXPR,
						.loc = peek(p).loc,
						.as.bin_expr.op =
							op == TOK_DOT   ? AST_OP_GET_FIELD :
							op == TOK_MINUS ? AST_OP_SUB       : 0,
					));
				} else {
					da_append(&nodes, ast(p,
						.kind = AST_UN_EXPR,
						.loc = peek(p).loc,
						.as.un_expr.op =
							op == TOK_DOT   ? AST_OP_FIELD :
							op == TOK_MINUS ? AST_OP_NEG   : 0,
					));
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
				da_append(&nodes, ast(p,
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
				));
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

	return expand(p, &nodes);
}

AST *func_ret(Parser *p) {
	AST *func = ast(p,
		.kind = AST_RET,
		.loc = next(p).loc);
	func->as.ret.expr = expr(p, PARSE_EXPR_STMT);
	return func;
}

AST *scope_body(Parser *p, bool isProg, bool skipScope) {
	if (!skipScope)
		symbol_table_push(p);

	bool isArr   = false;
	bool isArrEq = false;

	AST *body = ast(p,
		.kind = AST_BODY,
		.loc = peek(p).loc,
		.as.body.stmts = (ASTs){.arena = &p->arena},
		.as.body.scope = !skipScope,
	);

	if (!isProg) {
		if (!(peek(p).kind == TOK_OBRA ||
			peek(p).kind == TOK_ARROW  ||
			peek(p).kind == TOK_ARROW_EQ)) {
			parser_error(p, peek(p).loc, "invalid body declaration");
			return NULL;
		}
		if (peek(p).kind == TOK_ARROW)
			isArr = true;
		else if (peek(p).kind == TOK_ARROW_EQ)
			isArrEq = true;
		next(p);
	}

	ASTs *stmts = &body->as.body.stmts;
	while ((!isProg && peek(p).kind != TOK_CBRA) ||
		(isProg && peek(p).kind != TOK_EOF)) {
		if (p->err_ctx.got_err) return NULL;
		switch (peek(p).kind) {
			case TOK_FUNC:      da_append(stmts, func_def(p));   break;
			case TOK_RET:       da_append(stmts, func_ret(p));   break;
			case TOK_IF_SYM:    da_append(stmts, if_stmt(p));    break;
			case TOK_WHILE_SYM: da_append(stmts, while_stmt(p)); break;
			case TOK_FOR_SYM:   da_append(stmts, for_stmt(p));   break;

			case TOK_IMPORT: {
				next(p);
				expect(p, TOK_STRING);
				Parser ip = {
					.err_ctx = p->err_ctx,
					.symbols = p->symbols,
					.arena = p->arena,
					.lexer = lexer_from_file(&ip.arena, peek(p).data),
				};

				if (!ip.lexer.cur_char) {
					parser_error(p, peek(p).loc, "no such file");
					p->err_ctx.got_err = true;
					return NULL;
				}

				AST *ib = scope_body(&ip, true, true);
				if (ip.err_ctx.got_err) {
					p->err_ctx.got_err = true;
					return NULL;
				}

				da_foreach (AST*, it, &ib->as.body.stmts) {
					da_append(&body->as.body.stmts, *it);
				}

				p->symbols = ip.symbols;
				p->arena = ip.arena;
				lexer_free(&ip.lexer);
				next(p);
			} break;

			case TOK_ID: {
				if (peek2(p).kind == TOK_ASSIGN) {
					da_append(&body->as.body.stmts, var_def_assign(p));
				} else {
					da_append(&body->as.body.stmts, var_mut(p, PARSE_EXPR_STMT));
				}
			} break;

			case TOK_BREAK: {
				da_append(&body->as.body.stmts, ast(p,
					.kind = AST_BREAK,
					.loc = next(p).loc
				));
			} break;

			case TOK_CONTINUE: {
				da_append(&body->as.body.stmts, ast(p,
					.kind = AST_CONT,
					.loc = next(p).loc
				));
			} break;

			default: {
				if (peek(p).kind == TOK_ERR) {
					parser_error(p, peek(p).loc, peek(p).data);
					return NULL;
				}

				da_append(&body->as.body.stmts, var_mut(p, PARSE_EXPR_STMT));
				if(p->err_ctx.got_err) return NULL;
			}
		}

		if (isArr) break;
		if (isArrEq) {
			AST *var_mut = da_last(&body->as.body.stmts);
			if (var_mut->kind != AST_VAR_MUT) {
				parser_error(p, var_mut->loc, "expression expected");
				return NULL;
			}

			body->as.body.stmts.count = 0;
			da_append(&body->as.body.stmts, ast(p,
				.kind = AST_RET,
				.loc = body->loc,
				.as.ret = var_mut->as.var_mut,
			));
			break;
		}

		next(p);
	}

	if (!skipScope)
		symbol_table_pop(p);
	return body;
}

AST *parse(Parser *p) {
	return ast(p,
		.kind = AST_PROG,
		.loc = peek(p).loc,
		.as.prog.body = scope_body(p, true, false)
	);
}

void parser_free(Parser *p) {
	lexer_free(&p->lexer);
	arena_free(&p->arena);
	da_free(&p->symbols);
}
