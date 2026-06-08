#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include "../include/eval.h"

HT_IMPL(ValDict, Val, Val)
HT_IMPL_STR(RegSymbols, RegSymbol)

#define INVALID_COMB "invalid combination of operand and operators"

void eval_error(EvalCtx *ctx, Location loc, char *msg) {
	ctx->err_ctx.got_err = true;
	ctx->err_ctx.errf(&ctx->call_stack, loc, ERROR_RUNTIME, msg);
}

#define STACK_SIZE 1024
static size_t var_stack_ptrs[STACK_SIZE];
static size_t temp_stack_ptrs[STACK_SIZE];
static size_t call_stack_ptrs[STACK_SIZE];
static size_t stack_ptr = 0;

void eval_stack_push_scope(EvalCtx *ctx) {
	if (stack_ptr == STACK_SIZE) {
		eval_error(ctx,
			da_last(&ctx->call_stack).loc,
			"stack overflow");
		return;
	}

	var_stack_ptrs[stack_ptr] = ctx->var_stack.count;
	temp_stack_ptrs[stack_ptr] = ctx->temp_stack.count;
	call_stack_ptrs[stack_ptr++] = ctx->call_stack.count;
}

void eval_stack_pop_scope(EvalCtx *ctx) {
	ctx->var_stack.count = var_stack_ptrs[--stack_ptr];
	ctx->temp_stack.count = temp_stack_ptrs[stack_ptr];
	ctx->call_stack.count = call_stack_ptrs[stack_ptr];
}

void eval_stack_append(EvalCtx *ctx, AST_Var var) {
	da_append(&ctx->var_stack, ((EvalVar){.var = var, .val = VNONE}));
}

void eval_temp_stack_append(EvalCtx *ctx, Val val) {
	da_append(&ctx->temp_stack, val);
}

void eval_stack_set(EvalCtx *ctx, Location loc, AST_Var var, Val val) {
	if (var.uid != 0) {
		for (int i = (int)stack_ptr - 1; i >= 0; i--) {
			size_t idx = var_stack_ptrs[i];
			bool last = i == ((int)stack_ptr - 1);
			size_t next = last * ctx->var_stack.count +
			             !last * var_stack_ptrs[i + 1];
			if (var.idx >= (next - idx)) continue;
			EvalVar *ev = ctx->var_stack.items + idx + var.idx;
			if (ev->var.uid == var.uid) {
				ev->val = val;
				return;
			}
		}
	}

	if (var.id) {
		RegSymbol *rs = RegSymbols_get(&ctx->reg_sbls, var.id);
		if (rs) {
			rs->as.var = val;
			return;
		}
	}

	eval_error(ctx, loc, "no such variable / function");
}

Val eval_stack_get(EvalCtx *ctx, Location loc, AST_Var var) {
	if (var.uid != 0) {
		for (int i = (int)stack_ptr - 1; i >= 0; i--) {
			size_t idx = var_stack_ptrs[i];
			bool last = i == ((int)stack_ptr - 1);
			size_t next = last * ctx->var_stack.count +
			             !last * var_stack_ptrs[i + 1];
			if (var.idx >= (next - idx)) continue;
			EvalVar ev = ctx->var_stack.items[idx + var.idx];
			if (ev.var.uid == var.uid) return ev.val;
		}
	}

	if (var.id) {
		RegSymbol *rs = RegSymbols_get(&ctx->reg_sbls, var.id);
		if (rs) return rs->as.var;
	}

	eval_error(ctx, loc, "no such variable / function");
}

GC_Object *eval_gc_alloc(EvalCtx *ctx, int val_kind);

Val eval_make_val(EvalCtx *ctx, int kind) {
	Val hv = {
		.kind = kind,
		.as.gc_obj = eval_gc_alloc(ctx, kind),
	};

	eval_temp_stack_append(ctx, hv);
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

#define is_heap_val(vk) ( \
	(vk).kind == VAL_DICT || \
	(vk).kind == VAL_STR  || \
	(vk).kind == VAL_LIST)

Val eval_val_mut(EvalCtx *ctx, Location op_loc, AST_Op op, Val mut, Val to) {
	if ((is_heap_val(mut) || is_heap_val(to)) && op != AST_OP_EQ) {
		eval_error(ctx, op_loc, INVALID_COMB);
		return VNONE;
	}

#define VAL_MUT_CASE(ast_op, op) \
	case ast_op: \
		switch (mut.kind) { \
			case VAL_FLOAT: mut.as.vfloat op vget(to); break; \
			case VAL_INT:   mut.as.vint   op vget(to); break; \
			case VAL_BOOL:  mut.as.vbool  op vget(to); break; \
			case VAL_RUNE:  mut.as.vrune  op vget(to); break; \
			default: assert(0); \
		} break;
	switch (op) {
		VAL_MUT_CASE(AST_OP_ADD_EQ, +=);
		VAL_MUT_CASE(AST_OP_SUB_EQ, -=);
		VAL_MUT_CASE(AST_OP_MUL_EQ, *=);
		VAL_MUT_CASE(AST_OP_DIV_EQ, /=);
		default: mut = to;
	}
#undef VAL_MUT_CASE

	return mut;
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
		.as.field = var->as.var.id,
	};
}

Val eval_binop(EvalCtx *ctx, AST *n) {
	AST_Op op = n->as.bin_expr.op;
	Val lv = eval(ctx, n->as.bin_expr.lhs);
	if (ctx->err_ctx.got_err) return VNONE;

	AST *rhs = n->as.bin_expr.rhs;
	if (op == AST_OP_GET_FIELD) {
		if (lv.kind != VAL_DICT) {
			eval_error(ctx, n->as.bin_expr.lhs->loc, "object expected");
			return VNONE;
		}

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
	} else if (
		(lk == VAL_STR || lk == VAL_DICT || lk == VAL_FIELD || lk == VAL_LIST) &&
		(op == AST_OP_IS_EQ || op == AST_OP_NOT_EQ)
	) {
		int cmp = ValDict_compare(lv, rv);
		bool res =
			op == AST_OP_IS_EQ ?
			cmp == 0 : cmp != 0;

		return (Val){
			.kind = VAL_BOOL,
			.as.vbool = res,
		};
	} else if (
		op == AST_OP_IS_EQ || op == AST_OP_NOT_EQ   ||
		op == AST_OP_AND   || op == AST_OP_OR       ||
		op == AST_OP_GREAT || op == AST_OP_GREAT_EQ ||
		op == AST_OP_LESS  || op == AST_OP_LESS_EQ
	) {
		bool res;
		if (lk == VAL_NONE && rk != VAL_NONE) {
			res = false;
		} else if (lk != VAL_NONE && rk == VAL_NONE) {
			res = false;
		} else if (lk == VAL_NONE && rk == VAL_NONE) {
			if (op == AST_OP_IS_EQ || op == AST_OP_NOT_EQ) {
				res = op == AST_OP_IS_EQ;
			} else eval_error(ctx, n->loc, INVALID_COMB);
		} else res = binop(ctx, n->loc, op, vget(lv), vget(rv));

		return (Val){
			.kind = VAL_BOOL,
			.as.vbool = res,
		};
	} else if (lk == VAL_NONE || rk == VAL_NONE) {
		eval_error(ctx, n->loc, INVALID_COMB);
	} else if (lk == VAL_STR && rk == VAL_STR && op == AST_OP_ADD) {
		Val str = eval_make_val(ctx, VAL_STR);
		StringBuilder *sb = VSTR(str);
		sb_appendf(sb, "%s%s", VSTR(lv)->items, VSTR(rv)->items);
		return str;
	} else if (lk == VAL_LIST && rk == VAL_LIST && op == AST_OP_ADD) {
		Val list = eval_make_val(ctx, VAL_LIST);
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
			.as.field = n->as.un_expr.v->as.var.id,
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
			bool scope = n->as.body.scope;
			if (scope) eval_stack_push_scope(ctx);

			da_foreach (AST*, st, &n->as.body.stmts) {
				if (st == NULL) continue;
				Val res = eval(ctx, *st);
				if (ctx->err_ctx.got_err) return VNONE;
				if (ctx->state == EVAL_CTX_RET  ||
					ctx->state == EVAL_CTX_CONT ||
					ctx->state == EVAL_CTX_BREAK) {
					if (scope) eval_stack_pop_scope(ctx);
					if (ctx->state == EVAL_CTX_RET)
						eval_temp_stack_append(ctx, res);
					return res;
				}
			}

			if (scope) {
				eval_stack_pop_scope(ctx);
			}
		} break;

		case AST_VAR_DEF: {
			Val val = eval(ctx,
				n->as.var_def.expr);
			eval_stack_append(ctx,
				n->as.var_def.var);
			eval_stack_set(ctx,
				n->loc, n->as.var_def.var, val);
			if (ctx->err_ctx.got_err) return VNONE;
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
					Val str = eval_make_val(ctx, VAL_STR);
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
			Val list = eval_make_val(ctx, VAL_LIST);
			da_foreach (AST*, it, &n->as.list) {
				Val res = eval(ctx, *it);
				da_append(VLIST(list), res);
				if (ctx->err_ctx.got_err) return VNONE;
			}

			return list;
		} break;

		case AST_DICT: {
			Val dict = eval_make_val(ctx, VAL_DICT);
			da_foreach (AST*, it, &n->as.dict) {
				Val lv = eval(ctx, (*it)->as.bin_expr.lhs);
				if (ctx->err_ctx.got_err) return VNONE;
				Val rv = eval(ctx, (*it)->as.bin_expr.rhs);
				if (ctx->err_ctx.got_err) return VNONE;
				ValDict_add(VDICT(dict), lv, rv);
			}

			return dict;
		} break;

		case AST_VAR:
			return eval_stack_get(ctx, n->loc, n->as.var);

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
						if (!dict_val) {
							ValDict_add(VDICT(container), key, rhs_val);
						} else {
							*dict_val = eval_val_mut(
								ctx, n->loc,
								n->as.bin_expr.op,
								*dict_val,
								rhs_val);
						}
					} else if (lhs->kind == AST_BIN_EXPR && lhs->as.bin_expr.op == AST_OP_ARR) {
						Val container = eval(ctx, lhs->as.bin_expr.lhs);
						Val key = eval(ctx, lhs->as.bin_expr.rhs);
						if (ctx->err_ctx.got_err) return VNONE;

						if (container.kind == VAL_LIST) {
							check_index(ctx, n->loc, key.as.vint, VLIST(container)->count);
							if (ctx->err_ctx.got_err) return VNONE;

							Val *list_val = &da_get(VLIST(container), key.as.vint);
							*list_val = eval_val_mut(ctx, n->loc, n->as.bin_expr.op, *list_val, rhs_val);
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
							if (!dict_val) {
								ValDict_add(VDICT(container), key, rhs_val);
							} else {
								*dict_val = eval_val_mut(
									ctx, n->loc,
									n->as.bin_expr.op,
									*dict_val,
									rhs_val);
							}
						}
					} else if (lhs->kind == AST_VAR) {
						Val lhs_val = eval_stack_get(ctx,
							n->as.bin_expr.lhs->loc,
							n->as.bin_expr.lhs->as.var);
						if (ctx->err_ctx.got_err) return VNONE;

						eval_stack_set(ctx,
							n->loc,
							n->as.bin_expr.lhs->as.var,
							eval_val_mut(
								ctx,
								n->as.bin_expr.lhs->loc,
								n->as.bin_expr.op,
								lhs_val,
								rhs_val));
					} else {
						eval_error(ctx, n->loc, "EQ is used incorrectly");
						return VNONE;
					}
				} break;

				default: {
					return eval_binop(ctx, n);
				}
			}
		} break;

		case AST_FUNC_DEF: {
			eval_stack_append(ctx,
				n->as.func_def.var);
			eval_stack_set(
				ctx, n->loc,
				n->as.func_def.var,
				(Val){
					.kind = _VAL_FUNC,
					.as.func = n,
				});
			if (ctx->err_ctx.got_err)
				return VNONE;
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

			eval(ctx, n->as.st_for.var);
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
					eval_stack_pop_scope(ctx);
					return res;
				}

				eval(ctx, n->as.st_for.mut);
				if (ctx->err_ctx.got_err) return VNONE;
			}

			eval_stack_pop_scope(ctx);
		} break;

		case AST_ST_FOREACH: {
			AST_Var var = n->as.st_foreach.var;
			Val coll = eval(ctx, n->as.st_foreach.coll);
			if (ctx->err_ctx.got_err) return VNONE;

			if (coll.kind != VAL_LIST && coll.kind != VAL_STR && coll.kind != VAL_DICT) {
				eval_error(ctx, n->as.st_foreach.coll->loc, "expected list, string or dictionary");
				return VNONE;
			}

			if (coll.kind == VAL_DICT) {
				ht_foreach_node(ValDict, VDICT(coll), val) {
					eval_stack_push_scope(ctx);
					eval_stack_append(ctx, var);
					eval_stack_set(ctx, n->loc, var, val->key);
					if (ctx->err_ctx.got_err) return VNONE;

					Val res = eval(ctx, n->as.st_foreach.body);
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
					eval_stack_append(ctx, var);
					eval_stack_set(ctx, n->loc, var, x);
					if (ctx->err_ctx.got_err) return VNONE;

					Val res = eval(ctx, n->as.st_foreach.body);
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
			Val res;
			if (n->as.func_call.var.uid != 0) {
				eval_stack_push_scope(ctx);

				da_append(&ctx->call_stack, ((FuncCall){
					.loc = n->loc,
					.name = n->as.func_call.var.id,
				}));

				AST *func_def = eval_stack_get(
					ctx, n->loc,
					n->as.func_call.var)
					.as.func;
				if (ctx->err_ctx.got_err) return VNONE;

				bool found_any = false;
				AST_Var any_var = {0};
				Val va_args = {0};
				size_t args_cnt = 0;

				for (size_t i = 0; i < n->as.func_call.args.count; i++) {
					AST *func_call_arg = da_get(&n->as.func_call.args, i);

				found_any:
					if (found_any) {
						if (va_args.kind == VAL_NONE)
							va_args = eval_make_val(ctx, VAL_LIST);
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
						any_var = func_def_arg->as.var;
						goto found_any;
					}

					args_cnt++;
					Val val = eval(ctx, func_call_arg);
					eval_stack_append(ctx,
						func_def_arg->as.var);
					eval_stack_set(ctx,
						func_def_arg->loc,
						func_def_arg->as.var,
						val);
					if (ctx->err_ctx.got_err) {
						return VNONE;
					}
				}

				if (found_any) {
					eval_stack_append(ctx, any_var);
					eval_stack_set(ctx, n->loc, any_var, va_args);
					if (ctx->err_ctx.got_err) return VNONE;
				}

				if (!found_any && args_cnt < func_def->as.func_def.args.count) {
					eval_error(ctx, n->loc, "invalid amount of arguments");
					return VNONE;
				}

				ctx->state = EVAL_CTX_NONE; {
					res = eval(ctx, func_def->as.func_def.body);
					if (ctx->err_ctx.got_err) return VNONE;
				} ctx->state = EVAL_CTX_NONE;

				eval_stack_pop_scope(ctx);
			} else {
				RegSymbol *rf = RegSymbols_get(&ctx->reg_sbls, n->as.func_call.var.id);
				bool err = false;
				if (!rf) {
					err = true;
				} else if (rf->kind != REG_FUNC) {
					err = true;
				}

				if (err) {
					eval_error(ctx, n->loc, "no such function");
					return VNONE;
				}

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

				res = rf->as.func(ctx, n->loc, reg_func_args);
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

GC_Object *new_gc_obj(EvalCtx *ctx, int kind) {
	GC_Object *gco;

	if (ctx->gc.freed_objs.count > 0) {
		gco = da_last(&ctx->gc.freed_objs);
		da_remove_last(&ctx->gc.freed_objs);
		gco->val_kind = kind;
	} else {
		gco = malloc(sizeof(*gco));
		*gco = (GC_Object){
			.val_kind = kind,
			.data = malloc(sizeof(union{
				Vals          vals;
				ValDict       dict;
				EvalCustomObj cust;
				StringBuilder str;
			})),
		};
	}

	return gco;
}

void eval_gc_trigger(EvalCtx *ctx) {
	if (ctx->gc.threshold == 0)
		ctx->gc.threshold = GC_INIT_THRESHOLD;

	if (ctx->gc.alive_objs.count >= ctx->gc.threshold) {
		if (ctx->state != EVAL_CTX_RET) {
			eval_collect_garbage(ctx);
		}

		if (ctx->gc.alive_objs.count == 0) {
			ctx->gc.threshold = GC_INIT_THRESHOLD;
		} else {
			size_t v1 = ctx->gc.alive_objs.count * GC_GROWTH_FACTOR;
			size_t v2 = ctx->gc.alive_objs.count + GC_MIN_GROWTH;
			ctx->gc.threshold = v1 > v2 ? v1 : v2;
		}
	}
}

Val eval_gc_new_custom(EvalCtx *ctx, EvalCustomObj custom) {
	GC_Object *gco = new_gc_obj(ctx, VAL_CUSTOM);
	memcpy(gco->data, &custom, sizeof(custom));

	eval_gc_trigger(ctx);
	da_append(&ctx->gc.alive_objs, gco);

	Val val = {.kind = VAL_CUSTOM, .as.gc_obj = gco};
	eval_temp_stack_append(ctx, val);
	return val;
}

GC_Object *eval_gc_alloc(EvalCtx *ctx, int val_kind) {
	GC_Object *gco = new_gc_obj(ctx, val_kind);

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

	eval_gc_trigger(ctx);
	da_append(&ctx->gc.alive_objs, gco);
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
	da_foreach (GC_Object*, obj, &ctx->gc.alive_objs) {
		(*obj)->marked = false;
	}

	da_foreach (EvalVar, val, &ctx->var_stack) {
		if (is_heap_val(val->val)) {
			gc_obj_mark(val->val.as.gc_obj);
		}
	}

	da_foreach (Val, val, &ctx->temp_stack) {
		gc_obj_mark(val->as.gc_obj);
	}

	// sweep phase
	for (size_t i = 0; i < ctx->gc.alive_objs.count; i++) {
		GC_Object *obj = da_get(&ctx->gc.alive_objs, i);
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

				case VAL_CUSTOM: break;
				default: assert(0);
			}
		} else {
			if (obj->val_kind == VAL_CUSTOM) {
				EvalCustomObj *custom = obj->data;
				custom->free(custom->data);
			}
		}
	}

	for (size_t i = 0; i < ctx->gc.alive_objs.count; i++) {
		GC_Object *obj = da_get(&ctx->gc.alive_objs, i);
		if (!obj->marked) {
			da_append(&ctx->gc.freed_objs, obj);
			da_remove_unordered(&ctx->gc.alive_objs, i);
			i--;
		}
	}

	arena_reset(&ctx->gc.from);
	Arena temp = ctx->gc.from;
	ctx->gc.from = ctx->gc.to;
	ctx->gc.to = temp;
}

void eval_reg_var(EvalCtx *ctx, const char *id, Val val) {
	RegSymbols_add(&ctx->reg_sbls, (char*)id, (RegSymbol){
		.kind = REG_VAR,
		.as.var = val,
	});
}

void eval_reg_func(EvalCtx *ctx, const char *id, RegFunc rf) {
	RegSymbols_add(&ctx->reg_sbls, (char*)id, (RegSymbol){
		.kind = REG_FUNC,
		.as.func = rf,
	});
}

void eval_free(EvalCtx *ctx) {
	da_foreach (GC_Object*, obj, &ctx->gc.alive_objs) {
		free((*obj)->data);
		free(*obj);
	}

	da_foreach (GC_Object*, obj, &ctx->gc.freed_objs) {
		free((*obj)->data);
		free(*obj);
	}

	da_free(&ctx->gc.freed_objs);
	da_free(&ctx->gc.alive_objs);
	arena_free(&ctx->gc.from);
	arena_free(&ctx->gc.to);
	RegSymbols_free(&ctx->reg_sbls);
	da_free(&ctx->var_stack);
	da_free(&ctx->temp_stack);
	da_free(&ctx->call_stack);
}
