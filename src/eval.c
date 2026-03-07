#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include "../include/eval.h"

HT_IMPL(ValDict, Val, Val);
HT_IMPL(EvalScope, EvalScopeKey, EvalSymbol);

#define INVALID_COMB "invalid combination of operand and operators"

#define is_heap_val(vk) ( \
	(vk).kind == VAL_DICT || \
	(vk).kind == VAL_STR  || \
	(vk).kind == VAL_LIST)

u64 EvalScope_hashf(EvalScopeKey v) {
	if (!v.hs.str) return rand();
	return hash_combine(v.hs.hash, hash_num(v.kind));
}

int EvalScope_compare(EvalScopeKey a, EvalScopeKey b) {
	if (a.kind    != b.kind)    return 1;
	if (!a.hs.str || !b.hs.str) return 1;
	if (a.hs.hash != b.hs.hash) return 1;
	return strcmp(a.hs.str, b.hs.str);
}

void eval_error(EvalCtx *ctx, Location loc, char *msg) {
	ctx->err_ctx.got_err = true;
	ctx->err_ctx.errf(loc, ERROR_RUNTIME, msg);
}

GC_Object *eval_gc_alloc(EvalCtx *ctx, int val_kind);

void eval_stack_push_scope(EvalCtx *ctx) {
	da_append(&ctx->stack, (EvalScope){0});
}

void eval_stack_pop_scope(EvalCtx *ctx) {
	EvalScope_free(&da_last(&ctx->stack));
	ctx->stack.count--;
}

void eval_stack_add(EvalCtx *ctx, HashStr hs, EvalSymbol es, Location loc) {
	EvalScopeKey key = {0, hs};
	switch (es.kind) {
	case EVAL_SYMB_TEMP:
	case EVAL_SYMB_VAR:
		key.kind = EVAL_SKEY_VAR;
		break;
	case EVAL_SYMB_FUNC:
	case EVAL_SYMB_REG_FUNC:
		key.kind = EVAL_SKEY_FUNC;
		break;
	}

	if (EvalScope_get(&da_last(&ctx->stack), key)) {
		eval_error(ctx, loc, "redefinition of the variable");
	} else {
		EvalScope_add(&da_last(&ctx->stack), key, es);
	}
}

EvalSymbol *eval_stack_get(EvalCtx *ctx, int kind, HashStr var) {
	EvalScopeKey key = {kind, var};

	for (int i = ctx->stack.count - 1; i >= 0; i--) {
		EvalSymbol *smbl = EvalScope_get(&da_get(&ctx->stack, i), key);
		if (smbl) return smbl;
	}

	return NULL;
}

Val eval_new_heap_val(EvalCtx *ctx, int kind) {
	Val hv = {
		.kind = kind,
		.as.gc_obj = eval_gc_alloc(ctx, kind),
	};

	eval_stack_add(ctx, (HashStr){0}, (EvalSymbol){
		.kind = EVAL_SYMB_TEMP,
		.as.temp.val = hv,
	}, (Location){0});

	return hv;
}

void check_index(EvalCtx *ctx, Location loc, long long index, size_t count) {
	if (index < 0 || index >= count) {
		char err[512];
		sprintf(err,
			"index %lli is not in the range 0..%zu",
			index, count);
		eval_error(ctx, loc, err);
	}
}

u64 ValDict_hashf(Val key) {
	switch (key.kind) {
		case VAL_NONE:  return 0;
		case VAL_INT:   return hash_num(key.as.vint);
		case VAL_FLOAT: return hash_num(key.as.vint);
		case VAL_BOOL:  return hash_num(key.as.vbool);
		case VAL_FIELD: return hash_str(key.as.field);
		case VAL_STR:   return hash_str(VSTR(key)->items);
		case VAL_RUNE:  return hash_num(key.as.vrune);

		case VAL_LIST: {
			u64 hash = 0;
			da_foreach (Val, v, VLIST(key)) {
				hash_combine(hash, ValDict_hashf(*v));
			}

			return hash;
		} break;

		case VAL_DICT: {
			u64 hash = 0;
			ht_foreach_node (ValDict, VDICT(key), n) {
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
		case VAL_NONE:  return 0;
		case VAL_INT:   return a.as.vint != b.as.vint;
		case VAL_FLOAT: return a.as.vfloat != b.as.vfloat;
		case VAL_BOOL:  return a.as.vbool != b.as.vbool;
		case VAL_FIELD: return strcmp(a.as.field, b.as.field);
		case VAL_STR:   return strcmp(VSTR(a)->items, VSTR(b)->items);
		case VAL_RUNE:  return a.as.vrune != b.as.vrune;

		case VAL_LIST: {
			if (VLIST(a)->count != VLIST(b)->count)
				return 1;

			for (size_t i = 0; i < VLIST(a)->count; i++) {
				if (ValDict_compare(VLIST(a)->items[i], VLIST(b)->items[i]) != 0)
					return 1;
			}

			return 0;
		} break;

		case VAL_DICT: {
			if (VDICT(a)->count != VDICT(b)->count)
				return 1;

			ht_foreach_node (ValDict, VDICT(a), n) {
				Val *bv = ValDict_get(VDICT(b), n->key);
				if (!bv) return 1;
				if (ValDict_compare(n->val, *bv) != 0) return 1;
			}

			return 0;
		} break;
	}
}

#define vget(v) ( \
	(v).kind == VAL_INT   ? (v).as.vint   : \
	(v).kind == VAL_FLOAT ? (v).as.vfloat : \
	(v).kind == VAL_RUNE  ? (v).as.vrune  : \
	(v).kind == VAL_BOOL  ? (v).as.vbool  : 0)

#define binop(ctx, op_loc, op, l, r) ( \
	op == AST_OP_ADD      ? (l) +  (r) : \
	op == AST_OP_SUB      ? (l) -  (r) : \
	op == AST_OP_MUL      ? (l) *  (r) : \
	op == AST_OP_DIV      ? (l) /  (r) : \
	op == AST_OP_IS_EQ    ? (l) == (r) : \
	op == AST_OP_NOT_EQ   ? (l) != (r) : \
	op == AST_OP_GREAT    ? (l) >  (r) : \
	op == AST_OP_GREAT_EQ ? (l) >= (r) : \
	op == AST_OP_LESS     ? (l) <  (r) : \
	op == AST_OP_LESS_EQ  ? (l) <= (r) : \
	op == AST_OP_AND      ? (l) && (r) : \
	op == AST_OP_OR       ? (l) || (r) : \
	(eval_error(ctx, op_loc, "invalid operator"), 0))

#define unop(ctx, op_loc, op, v) (\
	op == AST_OP_NOT  ? !(v) : \
	op == AST_OP_NEG  ? -(v) : \
	(eval_error(ctx, op_loc, "invalid operator"), 0))

#define brk_cnt_ret(ctx, res) \
	if (ctx->state == EVAL_CTX_BREAK) { \
		ctx->state = EVAL_CTX_NONE; break; \
	} else if (ctx->state == EVAL_CTX_CONT) { \
		ctx->state = EVAL_CTX_NONE; \
	} else if (ctx->state == EVAL_CTX_RET) { \
		return res; \
	}

void eval_val_mut(EvalCtx *ctx, Location op_loc, AST_Op op, Val *mut, Val to) {
	if ((is_heap_val(*mut) || is_heap_val(to)) && op != AST_OP_EQ) {
		eval_error(ctx, op_loc, INVALID_COMB);
		return;
	}

#define VAL_MUT_CASE(ast_op, op) \
	case ast_op: \
		switch (mut->kind) { \
			case VAL_FLOAT: mut->as.vfloat op vget(to); break; \
			case VAL_INT:   mut->as.vint   op vget(to); break; \
			case VAL_BOOL:  mut->as.vbool  op vget(to); break; \
			case VAL_RUNE:  mut->as.vrune  op vget(to); break; \
			default: assert(0); \
		} break

	switch (op) {
		VAL_MUT_CASE(AST_OP_ADD_EQ, +=);
		VAL_MUT_CASE(AST_OP_SUB_EQ, -=);
		VAL_MUT_CASE(AST_OP_MUL_EQ, *=);
		VAL_MUT_CASE(AST_OP_DIV_EQ, /=);
		default: *mut = to;
	}
#undef VAL_MUT_CASE
}

int val_kind_to_prec(Val v) {
	switch (v.kind) {
		case VAL_BOOL:  return 1;
		case VAL_INT:   return 2;
		case VAL_RUNE:  return 3;
		case VAL_FLOAT: return 4;
		default:        return 0;
	}
}

int prec_to_val_kind(int pr) {
	switch (pr) {
		case 1:  return VAL_BOOL;
		case 2:  return VAL_INT;
		case 3:  return VAL_RUNE;
		case 4:  return VAL_FLOAT;
		default: return 0;
	}
}

Val eval_var_to_field(EvalCtx *ctx, AST *var) {
	if (var->kind != AST_VAR) {
		eval_error(ctx, var->loc, "field expected");
		return VNONE;
	}

	return (Val){
		.kind = VAL_FIELD,
		.as.field = var->as.var.str,
	};
}

Val eval_binop(EvalCtx *ctx, AST *n) {
	AST_Op op = n->as.bin_expr.op;
	Val lv = eval(ctx, n->as.bin_expr.lhs);
	if (ctx->err_ctx.got_err) return VNONE;

	AST *rhs = n->as.bin_expr.rhs;
	if (lv.kind == VAL_DICT && op == AST_OP_GET_FIELD) {
		Val rv = eval_var_to_field(ctx, n->as.bin_expr.rhs);
		if (ctx->err_ctx.got_err) return VNONE;
		Val *val = ValDict_get(VDICT(lv), rv);
		if (!val) return VNONE;
		return *val;
	}

	Val rv = eval(ctx, rhs);
	if (ctx->err_ctx.got_err) return VNONE;
	int lk = lv.kind, rk = rv.kind;

	if (op == AST_OP_MOD) {
		if (lk != VAL_INT || rk != VAL_INT) {
			eval_error(ctx, n->loc, INVALID_COMB);
			return VNONE;
		}

		return (Val){
			.kind = VAL_INT,
			.as.vint = lv.as.vint % rv.as.vint,
		};
	} else if (lk == VAL_STR && rk == VAL_STR && op == AST_OP_NOT_EQ) {
		return (Val){
			.kind = VAL_BOOL,
			.as.vbool = strcmp(VSTR(lv)->items, VSTR(rv)->items) != 0,
		};
	} else if (lk == VAL_STR && rk == VAL_STR && op == AST_OP_IS_EQ) {
		return (Val){
			.kind = VAL_BOOL,
			.as.vbool = strcmp(VSTR(lv)->items, VSTR(rv)->items) == 0,
		};
	} else if (lk == VAL_FIELD && rk == VAL_FIELD && op == AST_OP_IS_EQ) {
	return (Val){
			.kind = VAL_BOOL,
			.as.vbool = strcmp(lv.as.field, rv.as.field) == 0,
		};
	} else if (lk == VAL_FIELD && rk == VAL_FIELD && op == AST_OP_NOT_EQ) {
		return (Val){
			.kind = VAL_BOOL,
			.as.vbool = strcmp(lv.as.field, rv.as.field) != 0,
		};
	} else if (op == AST_OP_IS_EQ || op == AST_OP_NOT_EQ ||
		op == AST_OP_AND || op == AST_OP_OR ||
		op == AST_OP_GREAT || op == AST_OP_GREAT_EQ ||
		op == AST_OP_LESS || op == AST_OP_LESS_EQ) {
		bool res;

		if (lk == VAL_NONE && rk != VAL_NONE) {
			res = false;
		} else if (lk != VAL_NONE && rk == VAL_NONE) {
			res = false;
		} else if (lk == VAL_NONE && rk == VAL_NONE) {
			res = true;
		} else res = binop(ctx, n->loc, op, vget(lv), vget(rv));

		return (Val){
			.kind = VAL_BOOL,
			.as.vbool = res,
		};
	} else if (lk == VAL_NONE || rk == VAL_NONE) {
		eval_error(ctx, n->loc, INVALID_COMB);
	} else if (lk == VAL_STR && rk == VAL_STR && op == AST_OP_ADD) {
		Val str = eval_new_heap_val(ctx, VAL_STR);
		StringBuilder *sb = VSTR(str), *lvsb = VSTR(lv), *rvsb = VSTR(rv);
		sb_appendf(sb, "%s%s", VSTR(lv)->items, VSTR(rv)->items);
		return str;
	} else if (lk == VAL_LIST && rk == VAL_LIST && op == AST_OP_ADD) {
		Val list = eval_new_heap_val(ctx, VAL_LIST);
		da_append_many(VLIST(list), VLIST(lv)->items, VLIST(lv)->count);
		da_append_many(VLIST(list), VLIST(rv)->items, VLIST(rv)->count);
		return list;
	} else if (lk == VAL_INT && rk == VAL_INT && op == AST_OP_DIV) {
		return (Val){
			.kind = VAL_FLOAT,
			.as.vfloat = binop(ctx, n->loc, op, vget(lv), vget(rv))
		};
	} else if (lk == VAL_STR && op == AST_OP_ARR && rk == VAL_INT) {
		check_index(ctx, n->loc, rv.as.vint, VSTR(lv)->count);
		if (ctx->err_ctx.got_err) return VNONE;

		UTF8_Rune rune = utf8_get_nth(VSTR(lv)->items, rv.as.vint);
		return (Val){.kind = VAL_RUNE, .as.vrune = rune};
	} else if (lk == VAL_DICT && op == AST_OP_ARR) {
		Val *val = ValDict_get(VDICT(lv), rv);
		if (!val) return VNONE;
		return *val;
	} else if (lk == VAL_LIST && rk == VAL_INT && op == AST_OP_ARR) {
		check_index(ctx, n->loc, rv.as.vint, VLIST(lv)->count);
		if (ctx->err_ctx.got_err) return VNONE;

		return da_get(VLIST(lv), rv.as.vint);
	} else {
		if (lk != VAL_INT && lk != VAL_FLOAT && lk != VAL_BOOL && lk != VAL_RUNE) {
			eval_error(ctx, n->as.bin_expr.lhs->loc, INVALID_COMB);
			return VNONE;
		} else if (rk != VAL_INT && rk != VAL_FLOAT && rk != VAL_BOOL && rk != VAL_RUNE) {
			eval_error(ctx, n->as.bin_expr.rhs->loc, INVALID_COMB);
			return VNONE;
		}

		int lp = val_kind_to_prec(lv);
		int rp = val_kind_to_prec(rv);
		int retp = lp;
		if (rp > lp) retp = rp;

		switch (prec_to_val_kind(retp)) {
			case VAL_FLOAT: return (Val){
				.kind = VAL_FLOAT,
				.as.vfloat = binop(ctx, n->loc, op, vget(lv), vget(rv))
			};
			case VAL_RUNE: return (Val){
				.kind = VAL_RUNE,
				.as.vrune = binop(ctx, n->loc, op, vget(lv), vget(rv))
			};
			case VAL_INT: return (Val){
				.kind = VAL_INT,
				.as.vint = binop(ctx, n->loc, op, vget(lv), vget(rv))
			};
			case VAL_BOOL: return (Val){
				.kind = VAL_BOOL,
				.as.vbool = binop(ctx, n->loc, op, vget(lv), vget(rv))
			};
		}
	}

	return VNONE;
}

size_t val_get_count(Val coll) {
	switch (coll.kind) {
	case VAL_LIST:
		return VLIST(coll)->count;
	case VAL_STR:
		return utf8_len(VSTR(coll)->items);
	case VAL_DICT:
		return VDICT(coll)->count;
	default:
		return 0;
	}
}

Val eval_unop(EvalCtx *ctx, AST *n) {
	AST_Op op = n->as.un_expr.op;
	if (op == AST_OP_FIELD) {
		if (n->as.un_expr.v->kind != AST_VAR) {
			eval_error(ctx, n->as.un_expr.v->loc, "identifier expected");
			return VNONE;
		}

		return (Val){
			.kind = VAL_FIELD,
			.as.field = n->as.un_expr.v->as.var.str,
		};
	}

	Val v = eval(ctx, n->as.un_expr.v);
	if (ctx->err_ctx.got_err) return VNONE;

	if (v.kind != VAL_INT && v.kind != VAL_FLOAT && v.kind != VAL_BOOL) {
		eval_error(ctx, n->as.bin_expr.lhs->loc, INVALID_COMB);
		return VNONE;
	}

	switch (v.kind) {
		case VAL_FLOAT: return (Val){
			.kind = VAL_FLOAT,
			.as.vfloat = unop(ctx, n->loc, op, vget(v)),
		};

		case VAL_INT: return (Val){
			.kind = VAL_INT,
			.as.vint = unop(ctx, n->loc, op, vget(v)),
		};

		case VAL_BOOL: return (Val){
			.kind = VAL_BOOL,
			.as.vbool = unop(ctx, n->loc, op, vget(v)),
		};

		default: assert(0);
	}
}

Val eval(EvalCtx *ctx, AST *n) {
	if (ctx->err_ctx.got_err)
		return VNONE;

	switch (n->kind) {
		case AST_PROG:
			return eval(ctx, n->as.prog.body);

		case AST_BODY: {
			eval_stack_push_scope(ctx);
			da_foreach (AST*, st, &n->as.body) {
				if (st == NULL) continue;
				Val res = eval(ctx, *st);
				if (ctx->err_ctx.got_err) return VNONE;
				if (ctx->state == EVAL_CTX_RET ||
					ctx->state == EVAL_CTX_CONT ||
					ctx->state == EVAL_CTX_BREAK) {
					eval_stack_pop_scope(ctx);
					if (ctx->state == EVAL_CTX_RET) {
						eval_stack_add(ctx, (HashStr){0}, (EvalSymbol){
							.kind = EVAL_SYMB_TEMP,
							.as.temp.val = res,
						}, (Location){0});
					}
					return res;
				}
			}

			eval_stack_pop_scope(ctx);
		} break;

		case AST_VAR_DEF: {
			eval_stack_add(ctx, HS(n->as.var_def.id), (EvalSymbol){
				.kind = EVAL_SYMB_VAR,
				.as.var.val = eval(ctx, n->as.var_def.expr),
			}, n->loc);

			if (ctx->err_ctx.got_err)
				return VNONE;
		} break;

		case AST_VAL_NONE:
			return VNONE;

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
					Val str = eval_new_heap_val(ctx, VAL_STR);
					sb_appendf(VSTR(str), "%s", n->as.lit.as.vstr);
					return str;
				} break;

				case LITERAL_RUNE: {
					return (Val) {
						.kind = VAL_RUNE,
						.as.vrune = n->as.lit.as.vrune,
					};
				} break;

				default: assert(0);
			}
		} break;

		case AST_LIST: {
			Val list = eval_new_heap_val(ctx, VAL_LIST);
			da_foreach (AST*, it, &n->as.list) {
				Val res = eval(ctx, *it);
				da_append(VLIST(list), res);
				if (ctx->err_ctx.got_err) return VNONE;
			}

			return list;
		} break;

		case AST_DICT: {
			Val dict = eval_new_heap_val(ctx, VAL_DICT);
			da_foreach (AST*, it, &n->as.dict) {
				Val lv = eval(ctx, (*it)->as.bin_expr.lhs);
				if (ctx->err_ctx.got_err) return VNONE;
				Val rv = eval(ctx, (*it)->as.bin_expr.rhs);
				if (ctx->err_ctx.got_err) return VNONE;
				ValDict_add(VDICT(dict), lv, rv);
			}

			return dict;
		} break;

		case AST_VAR: {
			EvalSymbol *es = eval_stack_get(ctx, EVAL_SKEY_VAR, n->as.var);
			if (!es) {
				eval_error(ctx, n->loc, "no such variable");
				return VNONE;
			}

			return es->as.var.val;
		} break;

		case AST_UN_EXPR:
			return eval_unop(ctx, n);

		case AST_BIN_EXPR: {
			switch (n->as.bin_expr.op) {
				case AST_OP_ADD_EQ:
				case AST_OP_SUB_EQ:
				case AST_OP_MUL_EQ:
				case AST_OP_DIV_EQ:
				case AST_OP_EQ: {
					AST *lhs = n->as.bin_expr.lhs;
					AST *rhs = n->as.bin_expr.rhs;
					Val rhs_val = eval(ctx, rhs);
					if (ctx->err_ctx.got_err) return VNONE;

					if (lhs->kind == AST_BIN_EXPR && lhs->as.bin_expr.op == AST_OP_GET_FIELD) {
						Val container = eval(ctx, lhs->as.bin_expr.lhs);
						if (ctx->err_ctx.got_err) return VNONE;
						Val key = eval_var_to_field(ctx, lhs->as.bin_expr.rhs);
						if (ctx->err_ctx.got_err) return VNONE;

						Val *dict_val = ValDict_get(VDICT(container), key);
						if (!dict_val) ValDict_add(VDICT(container), key, rhs_val);
						else eval_val_mut(ctx, n->loc, n->as.bin_expr.op, dict_val, rhs_val);
					} else if (lhs->kind == AST_BIN_EXPR && lhs->as.bin_expr.op == AST_OP_ARR) {
						Val container = eval(ctx, lhs->as.bin_expr.lhs);
						Val key = eval(ctx, lhs->as.bin_expr.rhs);
						if (ctx->err_ctx.got_err) return VNONE;

						if (container.kind == VAL_LIST) {
							check_index(ctx, n->loc, key.as.vint, VLIST(container)->count);
							if (ctx->err_ctx.got_err) return VNONE;

							Val *list_val = &da_get(VLIST(container), key.as.vint);
							eval_val_mut(ctx, n->loc, n->as.bin_expr.op, list_val, rhs_val);
							if (ctx->err_ctx.got_err) return VNONE;
						} else if (container.kind == VAL_STR) {
							check_index(ctx, n->loc, key.as.vint, utf8_len(VSTR(container)->items));
							if (ctx->err_ctx.got_err) return VNONE;

							if (rhs_val.kind != VAL_RUNE) {
								eval_error(ctx, rhs->loc, "rune expected");
								return VNONE;
							}

							da_reserve(VSTR(container), VSTR(container)->count + 4);
							utf8_set_nth(VSTR(container)->items, key.as.vint, rhs_val.as.vrune);
							VSTR(container)->count = strlen(VSTR(container)->items);
						} else if (container.kind == VAL_DICT) {
							Val *dict_val = ValDict_get(VDICT(container), key);
							if (!dict_val) ValDict_add(VDICT(container), key, rhs_val);
							else eval_val_mut(ctx, n->loc, n->as.bin_expr.op, dict_val, rhs_val);
						}
					} else if (lhs->kind == AST_VAR) {
						HashStr var = n->as.bin_expr.lhs->as.var;
						EvalSymbol *es = eval_stack_get(ctx, EVAL_SKEY_VAR, var);
						if (!es) {
							eval_error(ctx, n->loc, "no such variable");
							return VNONE;
						}

						eval_val_mut(ctx, n->loc, n->as.bin_expr.op, &es->as.var.val, rhs_val);
					} else {
						eval_error(ctx, n->loc, "EQ is used incorrectly");
						return VNONE;
					}
				} break;

				default:
					return eval_binop(ctx, n);
			}
		} break;

		case AST_FUNC_DEF: {
			eval_stack_add(ctx, HS(n->as.func_def.id), (EvalSymbol){
				.kind = EVAL_SYMB_FUNC,
				.as.func.node = n,
			}, n->loc);
		} break;

		case AST_ST_ELSE:
			return eval(ctx, n->as.st_else.body);

		case AST_ST_IF: {
			Val cond = eval(ctx, n->as.st_if_chain.cond);
			if (ctx->err_ctx.got_err) return VNONE;
			if (cond.kind != VAL_BOOL) {
				eval_error(ctx, n->loc, "boolean expected");
				return VNONE;
			}

			if (cond.as.vbool) {
				return eval(ctx, n->as.st_if_chain.body);
			} else if (n->as.st_if_chain.chain) {
				return eval(ctx, n->as.st_if_chain.chain);
			}
		} break;

		case AST_ST_FOR: {
			eval_stack_push_scope(ctx);

			Val var = eval(ctx, n->as.st_for.var);
			if (ctx->err_ctx.got_err) return VNONE;

			for (;;) {
				Val cond = eval(ctx, n->as.st_for.cond);
				if (ctx->err_ctx.got_err) return VNONE;
				if (cond.kind != VAL_BOOL) {
					eval_error(ctx, n->loc, "boolean expected");
					return VNONE;
				}
				
				if (!cond.as.vbool) break;

				Val res = eval(ctx, n->as.st_for.body);
				if (ctx->err_ctx.got_err) return VNONE;
				if (ctx->state == EVAL_CTX_BREAK) {
					ctx->state = EVAL_CTX_NONE; break;
				} else if (ctx->state == EVAL_CTX_CONT) {
					ctx->state = EVAL_CTX_NONE;
				} else if (ctx->state == EVAL_CTX_RET) {
					return res;
				}

				eval(ctx, n->as.st_for.mut);
				if (ctx->err_ctx.got_err) return VNONE;
			}

			eval_stack_pop_scope(ctx);
		} break;

		case AST_ST_FOREACH: {
			char *var_id = n->as.st_foreach.var_id;
			Val coll = eval(ctx, n->as.st_foreach.coll);
			if (ctx->err_ctx.got_err) return VNONE;

			if (coll.kind != VAL_LIST && coll.kind != VAL_STR && coll.kind != VAL_DICT) {
				eval_error(ctx, n->as.st_foreach.coll->loc, "expected list, string or dictionary");
				return VNONE;
			}

			if (coll.kind == VAL_DICT) {
				ht_foreach_node(ValDict, VDICT(coll), val) {
					eval_stack_push_scope(ctx);
					eval_stack_add(ctx, HS(var_id), (EvalSymbol){
						.kind = EVAL_SYMB_VAR,
						.as.var.val = val->key,
					}, n->loc);

					Val res = eval(ctx, n->as.st_for.body);
					if (ctx->err_ctx.got_err) return VNONE;
					eval_stack_pop_scope(ctx);
					brk_cnt_ret(ctx, res);
				}
			} else {
				Val x = VNONE;
				size_t count = val_get_count(coll);

				for (size_t i = 0; i < count; i++) {
					count = val_get_count(coll);

					switch (coll.kind) {
					case VAL_LIST:
						x = VLIST(coll)->items[i];
						break;
					case VAL_STR:
						x = (Val){
							.kind = VAL_RUNE,
							.as.vrune = utf8_get_nth(VSTR(coll)->items, i),
						};
					}

					eval_stack_push_scope(ctx);
					eval_stack_add(ctx, HS(var_id), (EvalSymbol){
						.kind = EVAL_SYMB_VAR,
						.as.var.val = x,
					}, n->loc);

					Val res = eval(ctx, n->as.st_for.body);
					if (ctx->err_ctx.got_err) return VNONE;
					eval_stack_pop_scope(ctx);
					brk_cnt_ret(ctx, res);
				}
			}
		} break;

		case AST_ST_WHILE: {
			Val cond = eval(ctx, n->as.st_while.cond);
			if (ctx->err_ctx.got_err) return VNONE;
			if (cond.kind != VAL_BOOL) {
				eval_error(ctx, n->loc, "boolean expected");
				return VNONE;
			}

			while (true) {
				Val cond = eval(ctx, n->as.st_while.cond);
				if (cond.kind != VAL_BOOL) {
					eval_error(ctx, n->loc, "boolean expected");
					return VNONE;
				}

				if (!cond.as.vbool) break;

				Val res = eval(ctx, n->as.st_while.body);
				if (ctx->err_ctx.got_err) return VNONE;
				brk_cnt_ret(ctx, res);
			}
		} break;

		case AST_FUNC_CALL: {
			EvalSymbol *func = eval_stack_get(ctx, EVAL_SKEY_FUNC, HS(n->as.func_call.id));
			if (!func) {
				eval_error(ctx, n->loc, "no such function");
				return VNONE;
			}

			Val res;
			if (func->kind == EVAL_SYMB_FUNC) {
				eval_stack_push_scope(ctx);

				AST *func_def = func->as.func.node;
				bool found_any = false;
				Val va_args = {0};
				size_t args_cnt = 0;

				for (size_t i = 0; i < n->as.func_call.args.count; i++) {
					AST *func_call_arg = da_get(&n->as.func_call.args, i);

				found_any:
					if (found_any) {
						if (va_args.kind == VAL_NONE)
							va_args = eval_new_heap_val(ctx, VAL_LIST);
						Val va_arg = eval(ctx, func_call_arg);
						da_append(VLIST(va_args), va_arg);
						if (ctx->err_ctx.got_err) return VNONE;
						continue;
					}

					if (i >= func_def->as.func_def.args.count) {
						eval_error(ctx, n->loc, "invalid amount of arguments");
						return VNONE;
					}

					AST *func_def_arg = da_get(&func_def->as.func_def.args, i);
					if (func_def_arg->kind == AST_VAR_ANY) {
						found_any = true;
						goto found_any;
					}

					args_cnt++;
					eval_stack_add(ctx, func_def_arg->as.var, (EvalSymbol){
						.kind = EVAL_SYMB_VAR,
						.as.var.val = eval(ctx, func_call_arg),
					}, func_def_arg->loc);

					if (ctx->err_ctx.got_err)
						return VNONE;
				}

				if (found_any) {
					eval_stack_add(ctx, HS("_VA_ARGS_"), (EvalSymbol){
						.kind = EVAL_SYMB_VAR,
						.as.var.val = va_args,
					}, (Location){0});
				}

				if (!found_any && args_cnt < func_def->as.func_def.args.count) {
					eval_error(ctx, n->loc, "invalid amount of arguments");
					return VNONE;
				}

				ctx->state = EVAL_CTX_NONE; {
					res = eval(ctx, func->as.func.node->as.func_def.body);
					if (ctx->err_ctx.got_err) return VNONE;
				} ctx->state = EVAL_CTX_NONE;

				eval_stack_pop_scope(ctx);
			} else if (func->kind == EVAL_SYMB_REG_FUNC) {
				const int max_reg_func_args = 256;
				if (n->as.func_call.args.count > max_reg_func_args) {
					eval_error(ctx, n->loc, "too many arguments");
					return VNONE;
				}

				Vals temp_buf[max_reg_func_args];
				Vals reg_func_args = {
					.items = (Val*)temp_buf,
					.capacity = max_reg_func_args,
				};

				da_foreach (AST*, it, &n->as.func_call.args) {
					Val res = eval(ctx, *it);
					if (ctx->err_ctx.got_err) return VNONE;
					da_append(&reg_func_args, res);
				}

				res = func->as.reg_func(ctx, n->loc, reg_func_args);
				if (ctx->err_ctx.got_err) return VNONE;
			}

			return res;
		} break;

		case AST_RET: {
			if (n->as.ret.expr) {
				Val v = eval(ctx, n->as.ret.expr);
				ctx->state = EVAL_CTX_RET;
				return v;
			} else ctx->state = EVAL_CTX_RET;
		} break;

		case AST_BREAK: {
			ctx->state = EVAL_CTX_BREAK;
		} break;

		case AST_CONT: {
			ctx->state = EVAL_CTX_CONT;
		} break;

		case AST_VAR_MUT: {
			eval(ctx, n->as.var_mut);
		} break;

		default: assert(0);
	}

	return VNONE;
}

DA(GC_Object*) freed_objs = {0};

GC_Object *eval_gc_alloc(EvalCtx *ctx, int val_kind) {
	GC_Object *gco;
	if (freed_objs.count > 0) {
		gco = da_last(&freed_objs);
		da_remove_last(&freed_objs);
		gco->val_kind = val_kind;
	} else {
		gco = malloc(sizeof(*gco));
		*gco = (GC_Object){
			.val_kind = val_kind,
			.data = malloc(sizeof(union{
				Vals vals;
				ValDict dict;
				StringBuilder str;
			})),
		};
	}

	switch (val_kind) {
		case VAL_LIST: {
			*((Vals*)gco->data) = (Vals){0};
			da_set_arena((Vals*)gco->data, &ctx->gc.from);
		} break;

		case VAL_DICT: {
			*((ValDict*)gco->data) = (ValDict){0};
			ValDict_set_arena((ValDict*)gco->data, &ctx->gc.from);
		} break;

		case VAL_STR: {
			*((StringBuilder*)gco->data) = (StringBuilder){0};
			sb_set_arena((StringBuilder*)gco->data, &ctx->gc.from);
		} break;

		default: assert(0);
	}

	if (ctx->gc.threshold == 0)
		ctx->gc.threshold = GC_INIT_THRESHOLD;

	if (ctx->gc.objs.count >= ctx->gc.threshold) {
		if (ctx->state != EVAL_CTX_RET)
			eval_collect_garbage(ctx);

		if (ctx->gc.objs.count == 0) {
			ctx->gc.threshold = GC_INIT_THRESHOLD;
		} else {
			size_t v1 = ctx->gc.objs.count * GC_GROWTH_FACTOR;
			size_t v2 = ctx->gc.objs.count + GC_MIN_GROWTH;
			ctx->gc.threshold = v1 > v2 ? v1 : v2;
		}
	}

	da_append(&ctx->gc.objs, gco);
	return gco;
}

void gc_obj_mark(GC_Object *obj) {
	if (obj->marked) return;
	obj->marked = true;

	switch (obj->val_kind) {
		case VAL_DICT: {
			ValDict *dict = obj->data;
			ht_foreach_node (ValDict, dict, it) {
				if (is_heap_val(it->key))
					gc_obj_mark(it->key.as.gc_obj);
				if (is_heap_val(it->val))
					gc_obj_mark(it->val.as.gc_obj);
			}
		} break;

		case VAL_LIST: {
			Vals *list = obj->data;
			da_foreach (Val, it, list) {
				if (is_heap_val(*it)) {
					gc_obj_mark(it->as.gc_obj);
				}
			}
		} break;

		default:;
	}
}

void eval_collect_garbage(EvalCtx *ctx) {
	// mark phase
	da_foreach (GC_Object*, obj, &ctx->gc.objs) {
		(*obj)->marked = false;
	}

	da_foreach (EvalScope, scope, &ctx->stack) {
		ht_foreach_node (EvalScope, scope, esn) {
			EvalSymbol *es = &esn->val;
			if (es->kind == EVAL_SYMB_VAR) {
				if (is_heap_val(es->as.var.val)) {
					gc_obj_mark(es->as.var.val.as.gc_obj);
				}
			} else if (es->kind == EVAL_SYMB_TEMP) {
				gc_obj_mark(es->as.temp.val.as.gc_obj);
			}
		}
	}

	// sweep phase
	for (size_t i = 0; i < ctx->gc.objs.count; i++) {
		GC_Object *obj = da_get(&ctx->gc.objs, i);
		if (obj->marked) {
			switch (obj->val_kind) {
				case VAL_DICT: {
					ValDict *dict = obj->data;
					ValDict new = {0};
					ht_set_arena(&new, &ctx->gc.to);
					ht_foreach_node (ValDict, dict, n) {
						ValDict_add(&new, n->key, n->val);
					}
					*dict = new;
				} break;

				case VAL_LIST: {
					Vals *list = obj->data;
					Vals new = {0};
					da_set_arena(&new, &ctx->gc.to);
					da_append_many(&new, list->items, list->count);
					*list = new;
				} break;

				case VAL_STR: {
					StringBuilder *str = obj->data;
					StringBuilder new = {0};
					sb_set_arena(&new, &ctx->gc.to);
					sb_appendf(&new, "%s", str->items);
					*str = new;
				} break;

				default: assert(0);
			}
		}
	}

	for (size_t i = 0; i < ctx->gc.objs.count; i++) {
		GC_Object *obj = da_get(&ctx->gc.objs, i);
		if (!obj->marked) {
			da_append(&freed_objs, obj);
			da_remove_unordered(&ctx->gc.objs, i);
			i--;
		}
	}

	arena_reset(&ctx->gc.from);
	Arena temp = ctx->gc.from;
	ctx->gc.from = ctx->gc.to;
	ctx->gc.to = temp;
}

bool reg_scope = false;

void eval_reg_var(EvalCtx *ctx, const char *id, Val val) {
	if (!reg_scope) {
		da_insert(&ctx->stack, 0, (EvalScope){0});
		reg_scope = true;
	}

	EvalScope *scope = &da_get(&ctx->stack, 0);
	EvalScopeKey key = {EVAL_SKEY_VAR, HS((char*)id)};
	EvalScope_add(scope, key, ((EvalSymbol){
		.kind = EVAL_SYMB_VAR,
		.as.var.val = val,
	}));
}

void eval_reg_func(EvalCtx *ctx, const char *id, RegFunc rf) {
	if (!reg_scope) {
		da_insert(&ctx->stack, 0, (EvalScope){0});
		reg_scope = true;
	}
	
	EvalScope *scope = &da_get(&ctx->stack, 0);
	EvalScopeKey key = {EVAL_SKEY_FUNC, HS((char*)id)};
	EvalScope_add(scope, key, ((EvalSymbol){
		.kind = EVAL_SYMB_REG_FUNC,
		.as.reg_func = rf,
	}));
}
