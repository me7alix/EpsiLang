#include <assert.h>
#include "../include/eval.h"

HT_IMPL(ValDict, Val, Val)

u64 ValDict_hashf(Val key) {
	switch (key.kind) {
		case VAL_INT:
			return numhash(key.as.vint);

		case VAL_FLOAT:
			return numhash(key.as.vint);

		case VAL_BOOL:
			return numhash(key.as.vbool);

		case VAL_STR:
			return strhash(key.as.str->items);

		case VAL_LIST: {
			u64 hash = 0;
			da_foreach (Val, v, key.as.list) {
				hash_combine(hash, ValDict_hashf(*v));
			}

			return hash;
		} break;

		case VAL_DICT: {
			u64 hash = 0;
			ht_foreach_node (ValDict, (ValDict*)key.as.dict, n) {
				hash_combine(hash, ValDict_hashf(n->key));
				hash_combine(hash, ValDict_hashf(n->val));
			}

			return hash;
		} break;
	}
}

int ValDict_compare(Val a, Val b) {
	if (a.kind != b.kind) return 1;

	switch (a.kind) {
		case VAL_INT:
			return a.as.vint != b.as.vint;

		case VAL_FLOAT:
			return a.as.vfloat != b.as.vfloat;

		case VAL_BOOL:
			return a.as.vbool != b.as.vbool;

		case VAL_STR:
			return strcmp(a.as.str->items, b.as.str->items);

		case VAL_LIST: {
			if (a.as.list->count != b.as.list->count)
				return 1;

			for (size_t i = 0; i < a.as.list->count; i++) {
				if (ValDict_compare(a.as.list->items[i], b.as.list->items[i]) != 0)
					return 1;
			}

			return 0;
		} break;

		case VAL_DICT: {
			if (((ValDict*)a.as.dict)->count != ((ValDict*)a.as.dict)->count)
				return 1;

			ht_foreach_node (ValDict, (ValDict*)a.as.dict, n) {
				Val *bv = ValDict_get((ValDict*)b.as.dict, n->key);
				if (!bv) return 1;
				if (ValDict_compare(n->val, *bv) != 0) return 1;
			}

			return 0;
		} break;
	}
}

#define val_get(v) ( \
	(v).kind == VAL_INT   ? (v).as.vint   : \
	(v).kind == VAL_FLOAT ? (v).as.vfloat : \
	(v).kind == VAL_BOOL  ? (v).as.vbool  : \
	(assert(!"wrong val"), 0))

#define binop(op, l, r) \
	op == AST_OP_ADD      ? l +  r : \
	op == AST_OP_SUB      ? l -  r : \
	op == AST_OP_MUL      ? l *  r : \
	op == AST_OP_DIV      ? l /  r : \
	op == AST_OP_IS_EQ    ? l == r : \
	op == AST_OP_NOT_EQ   ? l != r : \
	op == AST_OP_GREAT    ? l >  r : \
	op == AST_OP_GREAT_EQ ? l >= r : \
	op == AST_OP_LESS     ? l <  r : \
	op == AST_OP_LESS_EQ  ? l <= r : \
	op == AST_OP_AND      ? l && r : \
	op == AST_OP_OR       ? l || r : \
	(assert(!"wrong binop"), 0)

Val eval_binop(AST_Op op, Val lv, Val rv) {
	int lk = lv.kind, rk = rv.kind;
	if (lk == VAL_STR && rk == VAL_STR && op == AST_OP_IS_EQ) {
		return (Val){
			.kind = VAL_BOOL,
			.as.vbool = strcmp(lv.as.str->items, rv.as.str->items) == 0,
		};
	} else if (op == AST_OP_IS_EQ || op == AST_OP_OR) {
		return (Val){VAL_BOOL, .as.vbool = binop(op, val_get(lv), val_get(rv))};
	} else if (lk == VAL_STR && rk == VAL_STR && op == AST_OP_ADD) {
		StringBuilder *str = malloc(sizeof(*str));
		*str = (StringBuilder){0};
		sb_appendf(str, "%s%s", lv.as.str->items, rv.as.str->items);

		return (Val){
			.kind = VAL_STR,
			.as.str = str,
		};
	} else if (lk == VAL_INT && rk == VAL_INT && op != AST_OP_DIV) {
		return (Val){VAL_INT, .as.vint = binop(op, val_get(lv), val_get(rv))};
	} else if (lk == VAL_DICT && op == AST_OP_ARR) {
		return *ValDict_get((ValDict*) lv.as.dict, rv);
	} else if (lk == VAL_LIST && rk == VAL_INT && op == AST_OP_ARR) {
		return da_get(lv.as.list, rv.as.vint);
	} else return (Val){VAL_FLOAT, .as.vfloat = binop(op, val_get(lv), val_get(rv))};
}

void eval_stack_add(EvalStack *es, EvalSymbol esmbl) {
	da_append(es, esmbl);
}

EvalSymbol *eval_stack_get(EvalStack *es, char *id) {
	for (ssize_t i = es->count - 1; i >= 0; i--) {
		if (strcmp(da_get(es, i).id, id) == 0) {
			return &da_get(es, i);
		}
	}

	return NULL;
}

Val eval(EvalCtx *ctx, AST *n) {
	switch (n->kind) {
		case AST_PROG:
			return eval(ctx, n->as.prog.body);

		case AST_BODY: {
			size_t stack_size = ctx->stack.count;
			da_foreach (AST*, it, &n->as.body) {
				Val res = eval(ctx, *it);
				if (ctx->state == EXEC_CTX_RET ||
					ctx->state == EXEC_CTX_CONT ||
					ctx->state == EXEC_CTX_BREAK) {
					ctx->stack.count = stack_size;
					return res;
				}
			}

			ctx->stack.count = stack_size;
		} break;

		case AST_VAR_DEF: {
			eval_stack_add(&ctx->stack, (EvalSymbol){
				.kind = EVAL_SYMB_VAR,
				.id = n->as.var_def.id,
				.as.var.val = eval(ctx, n->as.var_def.expr),
			});
		} break;

		case AST_LIT: {
			switch (n->as.lit.kind) {
				case LITERAL_INT:
					return (Val){
						.kind = VAL_INT,
						.as.vint = n->as.lit.as.vint,
					};

				case LITERAL_FLOAT:
					return (Val){
						.kind = VAL_FLOAT,
						.as.vfloat = n->as.lit.as.vfloat,
					};

				case LITERAL_BOOL:
					return (Val){
						.kind = VAL_BOOL,
						.as.vbool = n->as.lit.as.vbool,
					};

				case LITERAL_STR: {
					StringBuilder *str = malloc(sizeof(*str));
					*str = (StringBuilder){0};
					sb_appendf(str, "%s", n->as.lit.as.vstr);

					return (Val){
						.kind = VAL_STR,
						.as.str = str,
					};
				} break;

				default: assert(0);
			}
		} break;

		case AST_LIST: {
			Vals *list = malloc(sizeof(Vals));
			*list = (Vals){0};

			da_foreach (AST*, it, &n->as.list) {
				da_append(list, eval(ctx, *it));
			}

			return (Val){
				.kind = VAL_LIST,
				.as.list = list,
			};
		} break;

		case AST_DICT: {
			ValDict *dict = malloc(sizeof(ValDict));
			*dict = (ValDict){0};

			da_foreach (AST*, it, &n->as.dict) {
				Val lv = eval(ctx, (*it)->as.bin_expr.lhs);
				Val rv = eval(ctx, (*it)->as.bin_expr.rhs);
				ValDict_add(dict, lv, rv);
			}

			return (Val){
				.kind = VAL_DICT,
				.as.dict = dict,
			};
		} break;

		case AST_VAR:
			return eval_stack_get(&ctx->stack, n->as.var)->as.var.val;

		case AST_BIN_EXPR: {
			switch (n->as.bin_expr.op) {
				case AST_OP_EQ: {
					AST *lhs = n->as.bin_expr.lhs;
					AST *rhs = n->as.bin_expr.rhs;
					Val rhsv = eval(ctx, rhs);

					if (lhs->kind == AST_BIN_EXPR && lhs->as.bin_expr.op == AST_OP_ARR) {
						char *id = lhs->as.bin_expr.lhs->as.var;
						Val key = eval(ctx, lhs->as.bin_expr.rhs);
						EvalSymbol *es = eval_stack_get(&ctx->stack, id);
						if (es->as.var.val.kind == VAL_LIST) {
							da_get(es->as.var.val.as.list, key.as.vint) = eval(ctx, rhs);
						} else if (es->as.var.val.kind == VAL_DICT) {
							Val *val = ValDict_get((ValDict*)es->as.var.val.as.dict, key);
							if (!val) ValDict_add((ValDict*)es->as.var.val.as.dict, key, rhsv);
							else *val = rhsv;
						}
					} else if (n->kind == AST_VAR) {
						char *id = n->as.bin_expr.lhs->as.var;
						EvalSymbol *es = eval_stack_get(&ctx->stack, id);
						es->as.var.val = eval(ctx, n->as.bin_expr.rhs);
					}
				} break;

				default: {
					Val lv = eval(ctx, n->as.bin_expr.lhs);
					Val rv = eval(ctx, n->as.bin_expr.rhs);
					return eval_binop(n->as.bin_expr.op, lv, rv);
				}
			}
		} break;

		case AST_FUNC_DEF: {
			eval_stack_add(&ctx->stack, (EvalSymbol){
				.kind = EVAL_SYMB_FUNC,
				.id = n->as.func_def.id,
				.as.func.node = n,
			});
		} break;

		case AST_ST_ELSE: {
			return eval(ctx, n->as.st_else.body);
		} break;

		case AST_ST_IF: {
			Val cond = eval(ctx, n->as.st_if_chain.cond);
			if (cond.kind != VAL_BOOL)
				lexer_error(n->loc, "error: boolean expected");

			if (cond.as.vbool)
				return eval(ctx, n->as.st_if_chain.body);

			if (n->as.st_if_chain.chain)
				return eval(ctx, n->as.st_if_chain.chain);
		} break;

		case AST_ST_WHILE: {
			Val cond = eval(ctx, n->as.st_while.cond);
			if (cond.kind != VAL_BOOL)
				lexer_error(n->loc, "error: boolean expected");

			while (cond.as.vbool) {
				Val res = eval(ctx, n->as.st_while.body);
				if (ctx->state == EXEC_CTX_BREAK) {
					ctx->state = EXEC_CTX_NONE; break;
				} else if (ctx->state == EXEC_CTX_CONT) {
					ctx->state = EXEC_CTX_NONE;
				} else if (ctx->state == EXEC_CTX_RET) {
					return res;
				}
			}
		} break;

		case AST_FUNC_CALL: {
			EvalSymbol *f = eval_stack_get(&ctx->stack, n->as.func_call.id);

			Val res;
			if (f->kind == EVAL_SYMB_FUNC) {
				size_t stack_size = ctx->stack.count;
				for (size_t i = 0; i < n->as.func_call.args.count; i++) {
					char *var_id = da_get(&f->as.func.node->as.func_def.args, i);;
					AST *expr = da_get(&n->as.func_call.args, i);;

					eval_stack_add(&ctx->stack, (EvalSymbol){
						.kind = EVAL_SYMB_VAR,
						.id = var_id,
						.as.var.val = eval(ctx, expr),
					});
				}

				ctx->state = EXEC_CTX_NONE;
				res = eval(ctx, f->as.func.node->as.func_def.body);
				ctx->state = EXEC_CTX_NONE;
				ctx->stack.count = stack_size;
			} else if (f->kind == EVAL_SYMB_REG_FUNC) {
				Vals args = {0};
				da_foreach (AST*, it, &n->as.func_call.args)
				da_append(&args, eval(ctx, *it));
				res = f->as.reg_func(args);
			} else lexer_error(n->loc, "error: no such function");

			return res;
		} break;

		case AST_RET: {
			Val v = eval(ctx, n->as.ret.expr);;
			ctx->state = EXEC_CTX_RET;
			return v;
		} break;

		case AST_BREAK: {
			ctx->state = EXEC_CTX_BREAK;
		} break;

		case AST_CONT: {
			ctx->state = EXEC_CTX_CONT;
		} break;

		case AST_VAR_MUT: {
			eval(ctx, n->as.var_mut);
		} break;

		default: assert(0);
	}

	return (Val){0};
}

void reg_func(
	Parser *p, EvalCtx *ex,
	RegFunc rf, char *name,
	FuncArgsKind fk, size_t cnt
) {
	da_insert(&p->stack, 0, ((AST_Symbol){
		.kind = SYMBOL_FUNC,
		.id = name,
		.as.func.kind = fk,
		.as.func.count = cnt,
	}));

	da_insert(&ex->stack, 0, ((EvalSymbol){
		.kind = EVAL_SYMB_REG_FUNC,
		.id = name,
		.as.reg_func = rf,
	}));
}
