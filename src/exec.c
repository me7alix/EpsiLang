#include <assert.h>
#include <stdatomic.h>
#include "../include/exec.h"

#define exec_bin_op(op, lv, rv) \
if (lv.type.kind == TYPE_INT && rv.type.kind == TYPE_INT) { \
	if ((#op)[0] == '/') return (ExecCtx){0, (Val){ \
			.type = (Type){ TYPE_FLOAT }, \
			.as.vfloat = (double)lv.as.vint op rv.as.vint, \
	}}; \
	else return (ExecCtx){0, (Val){ \
		.type = (Type){ TYPE_INT }, \
		.as.vint = lv.as.vint op rv.as.vint, \
	}}; \
} else if (lv.type.kind == TYPE_INT && rv.type.kind == TYPE_FLOAT) { \
	return (ExecCtx){0, (Val){ \
		.type = (Type){ TYPE_FLOAT }, \
		.as.vfloat = lv.as.vint op rv.as.vfloat, \
	}}; \
} else if (lv.type.kind == TYPE_FLOAT && rv.type.kind == TYPE_INT) { \
	return (ExecCtx){0, (Val){ \
		.type = (Type){ TYPE_FLOAT }, \
		.as.vfloat = lv.as.vfloat op rv.as.vint, \
	}}; \
} else if (lv.type.kind == TYPE_FLOAT && rv.type.kind == TYPE_FLOAT) { \
	return (ExecCtx){0, (Val){ \
		.type = (Type){ TYPE_FLOAT }, \
		.as.vfloat = lv.as.vfloat op rv.as.vfloat, \
	}}; \
}

void exec_stack_add(ExecStack *es, ExecSymbol esmbl) {
	da_append(es, esmbl);
}

ExecSymbol *exec_stack_get(ExecStack *es, char *id) {
	for (ssize_t i = es->count - 1; i >= 0; i--) {
		if (strcmp(da_get(es, i).id, id) == 0) {
			return &da_get(es, i);
		}
	}

	return NULL;
}

ExecCtx exec(Exec *ex, AST *n) {
	switch (n->kind) {
		case AST_PROG:
			return exec(ex, n->as.prog.body);

		case AST_BODY: {
			size_t stack_size = ex->stack.count;
			da_foreach (AST*, it, &n->as.body) {
				ExecCtx res = exec(ex, *it);
				if (res.kind == EXEC_CTX_RET) {
					res.kind = EXEC_CTX_NONE;
					return res;
				}
			}

			ex->stack.count = stack_size;
		} break;

		case AST_VAR_DEF: {
			exec_stack_add(&ex->stack, (ExecSymbol){
				.kind = EXEC_SYMB_VAR,
				.id = n->as.var_def.id,
				.as.var.val = exec(ex, n->as.var_def.expr).val,
			});
		} break;

		case AST_LIT: {
			switch (n->as.lit.kind) {
				case LITERAL_INT: return (ExecCtx){0, (Val){
					.type = TYPE_INT,
					.as.vint = n->as.lit.as.int_val,
				}};
				case LITERAL_FLOAT: return (ExecCtx){0, (Val){
					.type = TYPE_FLOAT,
					.as.vfloat = n->as.lit.as.float_val,
				}};
				default: assert(0);
			}
		} break;

		case AST_VAR:
			return (ExecCtx){0, exec_stack_get(&ex->stack, n->as.var)->as.var.val};

		case AST_BIN_EXPR: {
			Val lv = exec(ex, n->as.bin_expr.lhs).val;
			Val rv = exec(ex, n->as.bin_expr.rhs).val;

			switch (n->as.bin_expr.op) {
				case AST_OP_ADD: exec_bin_op(+, lv, rv); break;
				case AST_OP_SUB: exec_bin_op(-, lv, rv); break;
				case AST_OP_MUL: exec_bin_op(*, lv, rv); break;
				case AST_OP_DIV: exec_bin_op(/, lv, rv); break;
				case AST_OP_EQ: {
					char *id = n->as.bin_expr.lhs->as.var;
					ExecSymbol *es = exec_stack_get(&ex->stack, id);
					es->as.var.val = rv;
				} break;
				default: assert(0);
			}
		} break;

		case AST_FUNC_DEF: {
			exec_stack_add(&ex->stack, (ExecSymbol){
				.kind = EXEC_SYMB_FUNC,
				.id = n->as.func_def.id,
				.as.func.node = n,
				.as.func.args_cnt = n->as.func_def.args.count,
			});
		} break;

		case AST_FUNC_CALL: {
			ExecSymbol *f = exec_stack_get(&ex->stack, n->as.func_call.id);

			ExecCtx res;
			if (f->kind == EXEC_SYMB_FUNC) {
				size_t stack_size = ex->stack.count;
				for (size_t i = 0; i < f->as.func.args_cnt; i++) {
					char *var_id = da_get(&f->as.func.node->as.func_def.args, i);;
					AST *expr = da_get(&n->as.func_call.args, i);;

					exec_stack_add(&ex->stack, (ExecSymbol){
						.kind = EXEC_SYMB_VAR,
						.id = var_id,
						.as.var.val = exec(ex, expr).val,
					});
				}

				res = exec(ex, f->as.func.node->as.func_def.body);
				ex->stack.count = stack_size;
			} else if (f->kind == EXEC_SYMB_REG_FUNC) {
				Vals args = {0};
				da_foreach (AST*, it, &n->as.func_call.args) {
					da_append(&args, exec(ex, *it).val);
				}

				res = (ExecCtx){0, f->as.reg_func(args)};
			} else lexer_error(n->loc, "error: no such function");

			return res;
		} break;

		case AST_RET: {
			ExecCtx ctx = exec(ex, n->as.ret.expr);
			ctx.kind = EXEC_CTX_RET;
			return ctx;
		} break;

		case AST_VAR_MUT: {
			exec(ex, n->as.var_mut);
		} break;

		default: assert(0);
	}

	return (ExecCtx){0};
}
