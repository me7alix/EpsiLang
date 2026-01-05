#include <assert.h>
#include <stddef.h>
#include "../include/eval.h"

HT_IMPL(ValDict, Val, Val);

#define is_heap_val(vk) ( \
	(vk) == VAL_DICT || \
	(vk) == VAL_STR || \
	(vk) == VAL_LIST)

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
	ctx->err.got_err = true;
	ctx->err.errf(loc, ERROR_RUNTIME, msg);
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
		case VAL_INT:   return numhash(key.as.vint);
		case VAL_FLOAT: return numhash(key.as.vint);
		case VAL_BOOL:  return numhash(key.as.vbool);
		case VAL_STR:   return strhash(VSTR(key)->items);

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

#define val_get(v) ( \
	(v).kind == VAL_INT   ? (v).as.vint   : \
	(v).kind == VAL_FLOAT ? (v).as.vfloat : \
	(v).kind == VAL_BOOL  ? (v).as.vbool  : \
	(assert(!"wrong val"), 0))

#define binop(ctx, op_loc, op, l, r) ( \
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
	(eval_error(ctx, op_loc, "wrong operator"), 0))

Val eval_binop(EvalCtx *ctx, AST *n) {
	AST_Op op = n->as.bin_expr.op;
	Val lv = eval(ctx, n->as.bin_expr.lhs);
	if (ctx->err.got_err) return NULL_VAL;
	Val rv = eval(ctx, n->as.bin_expr.rhs);
	if (ctx->err.got_err) return NULL_VAL;
	int lk = lv.kind, rk = rv.kind;

	if (lk == VAL_NONE && rk == VAL_NONE) {
		eval_error(ctx, n->loc, "wrong value");
	} else if (lk == VAL_STR && rk == VAL_STR && op == AST_OP_IS_EQ) {
		return (Val){
			.kind = VAL_BOOL,
			.as.vbool = strcmp(VSTR(lv)->items, VSTR(rv)->items) == 0,
		};
	} else if (op == AST_OP_IS_EQ || op == AST_OP_NOT_EQ ||
		op == AST_OP_AND || op == AST_OP_OR ||
		op == AST_OP_GREAT || op == AST_OP_GREAT_EQ ||
		op == AST_OP_LESS || op == AST_OP_LESS_EQ) {
		return (Val){
			.kind = VAL_BOOL,
			.as.vbool = binop(ctx, n->loc, op, val_get(lv), val_get(rv))
		};
	} else if (lk == VAL_STR && rk == VAL_STR && op == AST_OP_ADD) {
		Val str = eval_new_heap_val(ctx, VAL_STR);
		sb_appendf(VSTR(str), "%s%s", VSTR(lv)->items, VSTR(rv)->items);
		return str;
	} else if (lk == VAL_INT && rk == VAL_INT && op != AST_OP_DIV) {
		return (Val){VAL_INT, .as.vint = binop(ctx, n->loc, op, val_get(lv), val_get(rv))};
	} else if (lk == VAL_DICT && op == AST_OP_ARR) {
		return *ValDict_get(VDICT(lv), rv);
	} else if (lk == VAL_LIST && rk == VAL_INT && op == AST_OP_ARR) {
		return da_get(VLIST(lv), rv.as.vint);
	} else return (Val){
		.kind = VAL_FLOAT,
		.as.vfloat = binop(ctx, n->loc, op, val_get(lv), val_get(rv))
	};
}

Val eval(EvalCtx *ctx, AST *n) {
	if (ctx->err.got_err)
		return NULL_VAL;

	switch (n->kind) {
		case AST_PROG:
			return eval(ctx, n->as.prog.body);

		case AST_BODY: {
			size_t stack_size = ctx->stack.count;
			da_foreach (AST*, it, &n->as.body) {
				Val res = eval(ctx, *it);
				if (ctx->err.got_err) return NULL_VAL;
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

			if (ctx->err.got_err)
				return NULL_VAL;
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
				da_append(VLIST(list), eval(ctx, *it));
				if (ctx->err.got_err) return NULL_VAL;
			}

			return list;
		} break;

		case AST_DICT: {
			Val dict = eval_new_heap_val(ctx, VAL_DICT);
			da_foreach (AST*, it, &n->as.dict) {
				Val lv = eval(ctx, (*it)->as.bin_expr.lhs);
				if (ctx->err.got_err) return NULL_VAL;
				Val rv = eval(ctx, (*it)->as.bin_expr.rhs);
				if (ctx->err.got_err) return NULL_VAL;
				ValDict_add(VDICT(dict), lv, rv);
			}

			return dict;
		} break;

		case AST_VAR: {
			EvalSymbol *es = eval_stack_get(ctx, n->as.var);
			if (!es) {
				eval_error(ctx, n->loc, "no such symbol");
				return NULL_VAL;
			}

			if (es->kind != EVAL_SYMB_VAR)
				eval_error(ctx, n->loc, "no such variable");
			return es->as.var.val;
		} break;

		case AST_BIN_EXPR: {
			switch (n->as.bin_expr.op) {
				case AST_OP_EQ: {
					AST *lhs = n->as.bin_expr.lhs;
					AST *rhs = n->as.bin_expr.rhs;
					Val rhsv = eval(ctx, rhs);
					if (ctx->err.got_err) return NULL_VAL;

					if (lhs->kind == AST_BIN_EXPR && lhs->as.bin_expr.op == AST_OP_ARR) {
						char *var_id = lhs->as.bin_expr.lhs->as.var;
						EvalSymbol *es = eval_stack_get(ctx, var_id);
						if (!es) {
							eval_error(ctx, n->loc, "no such symbol");
							return NULL_VAL;
						} else if (es->kind != EVAL_SYMB_VAR) {
							eval_error(ctx, n->loc, "no such variable");
							return NULL_VAL;
						}

						Val key = eval(ctx, lhs->as.bin_expr.rhs);
						if (ctx->err.got_err) return NULL_VAL;

						if (es->as.var.val.kind == VAL_LIST) {
							da_get(VLIST(es->as.var.val), key.as.vint) = eval(ctx, rhs);
							if (ctx->err.got_err) return NULL_VAL;
						} else if (es->as.var.val.kind == VAL_DICT) {
							Val *val = ValDict_get(VDICT(es->as.var.val), key);
							if (!val) ValDict_add(VDICT(es->as.var.val), key, rhsv);
							else *val = rhsv;
						}
					} else if (lhs->kind == AST_VAR) {
						char *var_id = n->as.bin_expr.lhs->as.var;
						EvalSymbol *es = eval_stack_get(ctx, var_id);
						if (!es) {
							eval_error(ctx, n->loc, "no such symbol");
							return NULL_VAL;
						} else if (es->kind != EVAL_SYMB_VAR) {
							eval_error(ctx, n->loc, "no such variable");
							return NULL_VAL;
						}

						es->as.var.val = rhsv;
					} else {
						eval_error(ctx, n->loc, "EQ is used incorrectly");
						return NULL_VAL;
					}
				} break;

				default: {
					return eval_binop(ctx, n);
				}
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
			if (ctx->err.got_err) return NULL_VAL;
			if (cond.kind != VAL_BOOL) {
				eval_error(ctx, n->loc, "boolean expected");
				return NULL_VAL;
			}

			if (cond.as.vbool)
				return eval(ctx, n->as.st_if_chain.body);
			else if (n->as.st_if_chain.chain)
				return eval(ctx, n->as.st_if_chain.chain);
		} break;

		case AST_ST_FOR: {
			char *var_id = n->as.st_for.var_id;
			Val coll = eval(ctx, n->as.st_for.coll);
			if (ctx->err.got_err) return NULL_VAL;

			for (size_t i = 0; i < VLIST(coll)->count; i++) {
				Val x = VLIST(coll)->items[i];
				eval_stack_add(ctx, (EvalSymbol){
					.kind = EVAL_SYMB_VAR,
					.id = var_id,
					.as.var.val = x,
				});

				Val res = eval(ctx, n->as.st_for.body);
				if (ctx->err.got_err) return NULL_VAL;
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
			if (ctx->err.got_err) return NULL_VAL;
			if (cond.kind != VAL_BOOL) {
				eval_error(ctx, n->loc, "boolean expected");
				return NULL_VAL;
			}

			while (true) {
				Val cond = eval(ctx, n->as.st_while.cond);
				if (cond.kind != VAL_BOOL) {
					eval_error(ctx, n->loc, "boolean expected");
					return NULL_VAL;
				}

				if (!cond.as.vbool) break;

				Val res = eval(ctx, n->as.st_while.body);
				if (ctx->err.got_err) return NULL_VAL;
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
				return NULL_VAL;
			}

			AST *func_def = func->as.func.node;
			bool found_any = false;
			Val va_args = eval_new_heap_val(ctx, VAL_LIST);

			if (func->kind == EVAL_SYMB_FUNC) {
				size_t stack_size = ctx->stack.count;
				for (size_t i = 0; i < n->as.func_call.args.count; i++) {
					AST *func_call_arg = da_get(&n->as.func_call.args, i);

				found_any: if (found_any) {
						da_append(VLIST(va_args), eval(ctx, func_call_arg));
						if (ctx->err.got_err) return NULL_VAL;
						continue;
					}

					if (i >= func_def->as.func_def.args.count) {
						eval_error(ctx, n->loc, "wrong amount of arguments");
						return NULL_VAL;
					}

					AST *func_def_arg = da_get(&func_def->as.func_def.args, i);
					if (func_def_arg->kind == AST_VAR_ANY) {
						found_any = true;
						goto found_any;
					}

					eval_stack_add(ctx, (EvalSymbol){
						.kind = EVAL_SYMB_VAR,
						.id = func_def_arg->as.var,
						.as.var.val = eval(ctx, func_call_arg),
					});

					if (ctx->err.got_err)
						return NULL_VAL;
				}

				if (found_any) {
					eval_stack_add(ctx, (EvalSymbol){
						.kind = EVAL_SYMB_VAR,
						.id = "_VA_ARGS_",
						.as.var.val = va_args,
					});
				}

				ctx->state = EVAL_CTX_NONE;
				res = eval(ctx, func->as.func.node->as.func_def.body);
				if (ctx->err.got_err) return NULL_VAL;

				ctx->state = EVAL_CTX_NONE;
				ctx->stack.count = stack_size;
			} else if (func->kind == EVAL_SYMB_REG_FUNC) {
				Vals args = {0};
				da_foreach (AST*, it, &n->as.func_call.args) {
					da_append(&args, eval(ctx, *it));
					if (ctx->err.got_err) return NULL_VAL;
				}

				ErrorCtx ec = { .errf = ctx->err.errf };
				res = func->as.reg_func(ctx, n->loc, args);
				if (ec.got_err) ctx->err.got_err = true;
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

	return NULL_VAL;
}

GC_Object *eval_gc_alloc(EvalCtx *ctx, int val_kind) {
	GC_Object *gc_obj = malloc(sizeof(*gc_obj));
	gc_obj->val_kind = val_kind;

	switch (val_kind) {
		case VAL_LIST: {
			gc_obj->data = malloc(sizeof(Vals));
			*((Vals*)gc_obj->data) = (Vals){0};
		} break;

		case VAL_DICT: {
			gc_obj->data = malloc(sizeof(ValDict));
			*((ValDict*)gc_obj->data) = (ValDict){0};
		} break;

		case VAL_STR: {
			gc_obj->data = malloc(sizeof(StringBuilder));
			*((StringBuilder*)gc_obj->data) = (StringBuilder){0};
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

	da_append(&ctx->gc.objs, gc_obj);
	return gc_obj;
}

void gc_mark(GC_Object *obj) {
	if (!obj->marked) return;
	obj->marked = false;

	switch (obj->val_kind) {
		case VAL_DICT: {
			ValDict *dict = obj->data;
			ht_foreach_node (ValDict, dict, it) {
				if (is_heap_val(it->key.kind))
					gc_mark(it->key.as.gc_obj);
				if (is_heap_val(it->val.kind))
					gc_mark(it->val.as.gc_obj);
			}
		} break;

		case VAL_LIST: {
			Vals *list = obj->data;
			da_foreach (Val, it, list) {
				if (is_heap_val(it->kind))
					gc_mark(it->as.gc_obj);
			}
		} break;

		default:;
	}
}

void eval_collect_garbage(EvalCtx *ctx) {
	// mark phase
	da_foreach (GC_Object*, obj, &ctx->gc.objs) {
		(*obj)->marked = true;
	}

	da_foreach (EvalSymbol, es, &ctx->stack) {
		if (es->kind == EVAL_SYMB_VAR) {
			if (is_heap_val(es->as.var.val.kind)) {
				gc_mark(es->as.var.val.as.gc_obj);
			}
		} else if (es->kind == EVAL_SYMB_TEMP) {
			gc_mark(es->as.temp.val.as.gc_obj);
		}
	}

	// sweep phase
	for (size_t i = 0; i < ctx->gc.objs.count; i++) {
		GC_Object *obj = da_get(&ctx->gc.objs, i);
		if (obj->marked) {
			switch (obj->val_kind) {
				case VAL_DICT: {
					ValDict *dict = obj->data;
					ValDict_free(dict);
				} break;

				case VAL_LIST: {
					Vals *list = obj->data;
					da_free(list);
				} break;

				case VAL_STR: {
					StringBuilder *str = obj->data;
					sb_free(str);
				} break;

				default: assert(0);
			}

			free(obj->data);
			free(obj);
			da_remove_unordered(&ctx->gc.objs, i);
			i--;
		}
	}
}

void reg_var(EvalCtx *ctx, const char *id, Val val) {
	da_insert(&ctx->stack, 0, ((EvalSymbol){
		.kind = EVAL_SYMB_VAR,
		.id = (char*) id,
		.as.var.val = val,
	}));
}

void reg_func(EvalCtx *ctx, const char *id, RegFunc rf) {
	da_insert(&ctx->stack, 0, ((EvalSymbol){
		.kind = EVAL_SYMB_REG_FUNC,
		.id = (char*) id,
		.as.reg_func = rf,
	}));
}
