#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include "../include/eval.h"

HT_IMPL(ValDict, Val, Val);

#define is_heap_val(vk) ( \
	(vk).kind == VAL_DICT || \
	(vk).kind == VAL_STR || \
	(vk).kind == VAL_LIST)

GC_Object *eval_gc_alloc(EvalCtx *ctx, int val_kind);

void eval_stack_add(EvalCtx *ctx, EvalSymbol es) {
	da_append(&ctx->stack, es);
}

Val eval_new_heap_val(EvalCtx *ctx, u8 kind) {
	Val hv = {
		.kind = kind,
		.as.gc_obj = eval_gc_alloc(ctx, kind),
	};

	eval_stack_add(ctx, (EvalSymbol){
		.kind = EVAL_SYMB_TEMP,
		.id = "",
		.as.temp.val = hv,
	});

	return hv;
}

void eval_error(EvalCtx *ctx, Location loc, char *msg) {
	ctx->err_ctx.got_err = true;
	ctx->err_ctx.errf(loc, ERROR_RUNTIME, msg);
}

EvalSymbol *eval_stack_get(EvalCtx *es, char *id) {
	for (int i = es->stack.count - 1; i >= 0; i--) {
		if (strcmp(da_get(&es->stack, i).id, id) == 0) {
			return &da_get(&es->stack, i);
		}
	}

	return NULL;
}

u64 ValDict_hashf(Val key) {
	switch (key.kind) {
		case VAL_NONE:  return 0;
		case VAL_INT:   return hash_num(key.as.vint);
		case VAL_FLOAT: return hash_num(key.as.vint);
		case VAL_BOOL:  return hash_num(key.as.vbool);
		case VAL_STR:   return hash_str(VSTR(key)->items);

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
		case VAL_STR:   return strcmp(VSTR(a)->items, VSTR(b)->items);

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
	(v).kind == VAL_BOOL  ? (v).as.vbool  : \
	(assert(!"vget: wrong value"), 0))

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
	(eval_error(ctx, op_loc, "wrong operator"), 0))

void eval_val_mut(EvalCtx *ctx, Location op_loc, AST_Op op, Val *mut, Val to) {
	if ((is_heap_val(*mut) || is_heap_val(to)) && op != AST_OP_EQ) {
		eval_error(ctx, op_loc, "wrong combination of operator and operands");
		return;
	}

	switch (op) {
		case AST_OP_ADD_EQ:
			switch (mut->kind) {
				case VAL_FLOAT: mut->as.vfloat += vget(to); break;
				case VAL_INT:   mut->as.vint   += vget(to); break;
				case VAL_BOOL:  mut->as.vbool  += vget(to); break;
				default: assert(0);
			} break;
		case AST_OP_SUB_EQ:
			switch (mut->kind) {
				case VAL_FLOAT: mut->as.vfloat -= vget(to); break;
				case VAL_INT:   mut->as.vint   -= vget(to); break;
				case VAL_BOOL:  mut->as.vbool  -= vget(to); break;
				default: assert(0);
			} break;
		case AST_OP_MUL_EQ:
			switch (mut->kind) {
				case VAL_FLOAT: mut->as.vfloat *= vget(to); break;
				case VAL_INT:   mut->as.vint   *= vget(to); break;
				case VAL_BOOL:  mut->as.vbool  *= vget(to); break;
				default: assert(0);
			} break;
		case AST_OP_DIV_EQ:
			switch (mut->kind) {
				case VAL_FLOAT: mut->as.vfloat /= vget(to); break;
				case VAL_INT:   mut->as.vint   /= vget(to); break;
				case VAL_BOOL:  mut->as.vbool  /= vget(to); break;
				default: assert(0);
			} break;
		default: *mut = to;
	}
}

Val eval_binop(EvalCtx *ctx, AST *n) {
	AST_Op op = n->as.bin_expr.op;
	Val lv = eval(ctx, n->as.bin_expr.lhs);
	if (ctx->err_ctx.got_err) return VNONE;
	Val rv = eval(ctx, n->as.bin_expr.rhs);
	if (ctx->err_ctx.got_err) return VNONE;
	int lk = lv.kind, rk = rv.kind;

	if (op == AST_OP_MOD) {
		if (lk != VAL_INT || rk != VAL_INT) {
			eval_error(ctx, n->loc, "wrong value");
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
		eval_error(ctx, n->loc, "wrong value");
	} else if (lk == VAL_STR && rk == VAL_STR && op == AST_OP_ADD) {
		Val str = eval_new_heap_val(ctx, VAL_STR);
		sb_appendf(VSTR(str), "%s%s", VSTR(lv)->items, VSTR(rv)->items);
		return str;
	} else if (lk == VAL_INT && rk == VAL_INT && op != AST_OP_DIV) {
		return (Val){.kind = VAL_INT, .as.vint = binop(ctx, n->loc, op, vget(lv), vget(rv))};
	} else if (lk == VAL_DICT && op == AST_OP_ARR) {
		Val *val = ValDict_get(VDICT(lv), rv);
		if (!val) return VNONE;
		return *val;
	} else if (lk == VAL_LIST && rk == VAL_INT && op == AST_OP_ARR) {
		if (rv.as.vint < 0 || rv.as.vint > VLIST(lv)->count) {
			char err[512];
			sprintf(err, "index %lli is not in the range 0..%zu", rv.as.vint, VLIST(lv)->count);
			eval_error(ctx, n->loc, err);
			return VNONE;
		}

		return da_get(VLIST(lv), rv.as.vint);
	} else return (Val){
		.kind = VAL_FLOAT,
		.as.vfloat = binop(ctx, n->loc, op, vget(lv), vget(rv))
	};

	return VNONE;
}

Val eval(EvalCtx *ctx, AST *n) {
	if (ctx->err_ctx.got_err)
		return VNONE;

	switch (n->kind) {
		case AST_PROG:
			return eval(ctx, n->as.prog.body);

		case AST_BODY: {
			size_t stack_size = ctx->stack.count;
			da_foreach (AST*, st, &n->as.body) {
				if (st == NULL) continue;
				Val res = eval(ctx, *st);
				if (ctx->err_ctx.got_err) return VNONE;
				if (ctx->state == EVAL_CTX_RET ||
					ctx->state == EVAL_CTX_CONT ||
					ctx->state == EVAL_CTX_BREAK) {
					ctx->stack.count = stack_size;
					return res;
				}
			}

			ctx->stack.count = stack_size;
		} break;

		case AST_VAR_DEF: {
			eval_stack_add(ctx, (EvalSymbol){
				.kind = EVAL_SYMB_VAR,
				.id = n->as.var_def.id,
				.as.var.val = eval(ctx, n->as.var_def.expr),
			});

			if (ctx->err_ctx.got_err)
				return VNONE;
		} break;

		case AST_VAL_NONE: {
			return VNONE;
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
					Val str = eval_new_heap_val(ctx, VAL_STR);
					sb_appendf(VSTR(str), "%s", n->as.lit.as.vstr);
					return str;
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
			EvalSymbol *es = eval_stack_get(ctx, n->as.var);
			if (!es) {
				eval_error(ctx, n->loc, "no such symbol");
				return VNONE;
			}

			if (es->kind != EVAL_SYMB_VAR)
				eval_error(ctx, n->loc, "no such variable");
			return es->as.var.val;
		} break;

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

					if (lhs->kind == AST_BIN_EXPR && lhs->as.bin_expr.op == AST_OP_ARR) {
						char *var_id = lhs->as.bin_expr.lhs->as.var;
						EvalSymbol *es = eval_stack_get(ctx, var_id);
						if (!es) {
							eval_error(ctx, n->loc, "no such symbol");
							return VNONE;
						} else if (es->kind != EVAL_SYMB_VAR) {
							eval_error(ctx, n->loc, "no such variable");
							return VNONE;
						}

						Val key = eval(ctx, lhs->as.bin_expr.rhs);
						if (ctx->err_ctx.got_err) return VNONE;

						if (es->as.var.val.kind == VAL_LIST) {
							if (key.as.vint < 0 || key.as.vint > VLIST(es->as.var.val)->count) {
								char err[512];
								sprintf(err,
									"index %lli is not in the range 0..%zu",
									key.as.vint, VLIST(es->as.var.val)->count);
								eval_error(ctx, n->as.bin_expr.lhs->loc, err);
								return VNONE;
							}

							Val *list_val = &da_get(VLIST(es->as.var.val), key.as.vint);
							eval_val_mut(ctx, n->loc, n->as.bin_expr.op, list_val, rhs_val);
							if (ctx->err_ctx.got_err) return VNONE;
						} else if (es->as.var.val.kind == VAL_DICT) {
							Val *dict_val = ValDict_get(VDICT(es->as.var.val), key);
							if (!dict_val) ValDict_add(VDICT(es->as.var.val), key, rhs_val);
							else eval_val_mut(ctx, n->loc, n->as.bin_expr.op, dict_val, rhs_val);
						}
					} else if (lhs->kind == AST_VAR) {
						char *var_id = n->as.bin_expr.lhs->as.var;
						EvalSymbol *es = eval_stack_get(ctx, var_id);
						if (!es) {
							eval_error(ctx, n->loc, "no such symbol");
							return VNONE;
						} else if (es->kind != EVAL_SYMB_VAR) {
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
			eval_stack_add(ctx, (EvalSymbol){
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
			if (ctx->err_ctx.got_err) return VNONE;
			if (cond.kind != VAL_BOOL) {
				eval_error(ctx, n->loc, "boolean expected");
				return VNONE;
			}

			if (cond.as.vbool)
				return eval(ctx, n->as.st_if_chain.body);
			else if (n->as.st_if_chain.chain)
				return eval(ctx, n->as.st_if_chain.chain);
		} break;

		case AST_ST_FOR: {
			size_t stack_count_before = ctx->stack.count;
			Val var = eval(ctx, n->as.st_for.var);
			if (ctx->err_ctx.got_err) return VNONE;
			bool remove_last = ctx->stack.count != stack_count_before;

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

			if (remove_last)
				ctx->stack.count--;
		} break;

		case AST_ST_FOREACH: {
			char *var_id = n->as.st_foreach.var_id;
			Val coll = eval(ctx, n->as.st_foreach.coll);
			if (ctx->err_ctx.got_err) return VNONE;

			for (size_t i = 0; i < VLIST(coll)->count; i++) {
				Val x = VLIST(coll)->items[i];
				eval_stack_add(ctx, (EvalSymbol){
					.kind = EVAL_SYMB_VAR,
					.id = var_id,
					.as.var.val = x,
				});

				Val res = eval(ctx, n->as.st_for.body);
				if (ctx->err_ctx.got_err) return VNONE;
				ctx->stack.count--;
				if (ctx->state == EVAL_CTX_BREAK) {
					ctx->state = EVAL_CTX_NONE; break;
				} else if (ctx->state == EVAL_CTX_CONT) {
					ctx->state = EVAL_CTX_NONE;
				} else if (ctx->state == EVAL_CTX_RET) {
					return res;
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
				if (ctx->state == EVAL_CTX_BREAK) {
					ctx->state = EVAL_CTX_NONE; break;
				} else if (ctx->state == EVAL_CTX_CONT) {
					ctx->state = EVAL_CTX_NONE;
				} else if (ctx->state == EVAL_CTX_RET) {
					return res;
				}
			}
		} break;

		case AST_FUNC_CALL: {
			Val res;
			EvalSymbol *func = eval_stack_get(ctx, n->as.func_call.id);
			if (!func) {
				eval_error(ctx, n->loc, "no such symbol");
				return VNONE;
			}

			if (func->kind == EVAL_SYMB_FUNC) {
				size_t stack_size = ctx->stack.count;
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
						eval_error(ctx, n->loc, "wrong amount of arguments");
						return VNONE;
					}

					AST *func_def_arg = da_get(&func_def->as.func_def.args, i);
					if (func_def_arg->kind == AST_VAR_ANY) {
						found_any = true;
						goto found_any;
					}

					args_cnt++;
					eval_stack_add(ctx, (EvalSymbol){
						.kind = EVAL_SYMB_VAR,
						.id = func_def_arg->as.var,
						.as.var.val = eval(ctx, func_call_arg),
					});

					if (ctx->err_ctx.got_err)
						return VNONE;
				}

				if (found_any) {
					eval_stack_add(ctx, (EvalSymbol){
						.kind = EVAL_SYMB_VAR,
						.id = "_VA_ARGS_",
						.as.var.val = va_args,
					});
				}

				if (!found_any && args_cnt < func_def->as.func_def.args.count) {
					eval_error(ctx, n->loc, "wrong amount of arguments");
					return VNONE;
				}

				ctx->state = EVAL_CTX_NONE;
				res = eval(ctx, func->as.func.node->as.func_def.body);
				if (ctx->err_ctx.got_err) return VNONE;

				ctx->state = EVAL_CTX_NONE;
				ctx->stack.count = stack_size;
			} else if (func->kind == EVAL_SYMB_REG_FUNC) {
				Vals args = {0};
				da_foreach (AST*, it, &n->as.func_call.args) {
					Val res = eval(ctx, *it);
					da_append(&args, res);
					if (ctx->err_ctx.got_err) return VNONE;
				}

				ErrorCtx ec = { .errf = ctx->err_ctx.errf };
				res = func->as.reg_func(ctx, n->loc, args);
				if (ec.got_err) ctx->err_ctx.got_err = true;
			} else eval_error(ctx, n->loc, "no such function");

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

GC_Object *eval_gc_alloc(EvalCtx *ctx, int val_kind) {
	GC_Object *gco = malloc(sizeof(*gco));
	*gco = (GC_Object){.val_kind = val_kind};

	switch (val_kind) {
		case VAL_LIST: {
			gco->data = malloc(sizeof(Vals));
			*((Vals*)gco->data) = (Vals){0};
			da_set_arena((Vals*)gco->data, &ctx->gc.from);
		} break;

		case VAL_DICT: {
			gco->data = malloc(sizeof(ValDict));
			*((ValDict*)gco->data) = (ValDict){0};
			ht_set_arena((ValDict*)gco->data, &ctx->gc.from);
		} break;

		case VAL_STR: {
			gco->data = malloc(sizeof(StringBuilder));
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
				if (is_heap_val(*it))
					gc_obj_mark(it->as.gc_obj);
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

	da_foreach (EvalSymbol, es, &ctx->stack) {
		if (es->kind == EVAL_SYMB_VAR) {
			if (is_heap_val(es->as.var.val)) {
				gc_obj_mark(es->as.var.val.as.gc_obj);
			}
		} else if (es->kind == EVAL_SYMB_TEMP) {
			gc_obj_mark(es->as.temp.val.as.gc_obj);
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
			free(obj->data);
			free(obj);
			da_remove_unordered(&ctx->gc.objs, i);
			i--;
		}
	}

	arena_free(&ctx->gc.from);

	Arena temp = ctx->gc.from;
	ctx->gc.from = ctx->gc.to;
	ctx->gc.to = temp;
}

void eval_reg_var(EvalCtx *ctx, const char *id, Val val) {
	da_insert(&ctx->stack, 0, ((EvalSymbol){
		.kind = EVAL_SYMB_VAR,
		.id = (char*) id,
		.as.var.val = val,
	}));
}

void eval_reg_func(EvalCtx *ctx, const char *id, RegFunc rf) {
	da_insert(&ctx->stack, 0, ((EvalSymbol){
		.kind = EVAL_SYMB_REG_FUNC,
		.id = (char*) id,
		.as.reg_func = rf,
	}));
}
