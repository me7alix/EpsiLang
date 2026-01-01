#include <assert.h>
#include "../include/eval.h"

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
	if (op == AST_OP_IS_EQ || op == AST_OP_OR) {
		return (Val){VAL_BOOL, .as.vbool = binop(op, val_get(lv), val_get(rv))};
	} else if (lk == VAL_INT && rk == VAL_INT && op != AST_OP_DIV) {
		return (Val){VAL_INT, .as.vint = binop(op, val_get(lv), val_get(rv))};
	} else if (lk == VAL_LIST && rk == VAL_INT && op == AST_OP_ARR) {
		return da_get(lv.as.list, rv.as.vint);
	} else return (Val){VAL_FLOAT, .as.vfloat = binop(op, val_get(lv), val_get(rv))};
}

void exec_stack_add(EvalStack *es, EvalSymbol esmbl) {
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
				if (ctx->state == EXEC_CTX_RET) {
					ctx->stack.count = stack_size;
					return res;
				}
			}

			ctx->stack.count = stack_size;
		} break;

		case AST_VAR_DEF: {
			exec_stack_add(&ctx->stack, (EvalSymbol){
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

		case AST_VAR:
			return eval_stack_get(&ctx->stack, n->as.var)->as.var.val;

		case AST_BIN_EXPR: {
			switch (n->as.bin_expr.op) {
				case AST_OP_EQ: {
					AST *lhs = n->as.bin_expr.lhs;
					AST *rhs = n->as.bin_expr.rhs;

					if (lhs->kind == AST_BIN_EXPR && lhs->as.bin_expr.op == AST_OP_ARR) {
						char *id = lhs->as.bin_expr.lhs->as.var;
						Val ind = eval(ctx, lhs->as.bin_expr.rhs);
						EvalSymbol *es = eval_stack_get(&ctx->stack, id);
						da_get(es->as.var.val.as.list, ind.as.vint) = eval(ctx, rhs);
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
			exec_stack_add(&ctx->stack, (EvalSymbol){
				.kind = EVAL_SYMB_FUNC,
				.id = n->as.func_def.id,
				.as.func.node = n,
			});
		} break;

		case AST_ST_IF: {
			Val cond = eval(ctx, n->as.st_if.cond);
			if (cond.kind != VAL_BOOL)
				lexer_error(n->loc, "error: boolean expected");

			if (cond.as.vbool)
				return eval(ctx, n->as.st_if.body);
		} break;

		case AST_FUNC_CALL: {
			EvalSymbol *f = eval_stack_get(&ctx->stack, n->as.func_call.id);

			Val res;
			if (f->kind == EVAL_SYMB_FUNC) {
				size_t stack_size = ctx->stack.count;
				for (size_t i = 0; i < n->as.func_call.args.count; i++) {
					char *var_id = da_get(&f->as.func.node->as.func_def.args, i);;
					AST *expr = da_get(&n->as.func_call.args, i);;

					exec_stack_add(&ctx->stack, (EvalSymbol){
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

		case AST_VAR_MUT: {
			eval(ctx, n->as.var_mut);
		} break;

		default: assert(0);
	}

	return (Val){0};
}

void epsl_reg_func(
	Parser *p, EvalCtx *ex,
	RegFunc rf, char *name,
	FuncArgsKind fk, size_t cnt
) {
	da_insert(&p->stack, 0, ((Symbol){
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
