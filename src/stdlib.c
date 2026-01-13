#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../include/eval.h"

#define bstr(b) (b ? "true" : "false")

#define err(ec, loc, msg) \
do { \
	(ec)->err_ctx.got_err = true; \
	(ec)->err_ctx.errf(loc, ERROR_RUNTIME, msg); \
	return VNONE; \
} while(0)

void val_sprint_f(Val v, char *buf, int depth) {
	switch (v.kind) {
		case VAL_NONE:  sprintf(buf, "none");                 break;
		case VAL_INT:   sprintf(buf, "%lli", v.as.vint);      break;
		case VAL_FLOAT: sprintf(buf, "%lf", v.as.vfloat);     break;
		case VAL_BOOL:  sprintf(buf, "%s", bstr(v.as.vbool)); break;

		case VAL_STR:
			if (depth == 0)
				sprintf(buf, "%s", VSTR(v)->items);
			else
				sprintf(buf, "\"%s\"", VSTR(v)->items);
			break;

		case VAL_LIST: {
			StringBuilder sb = {0};
			sb_appendf(&sb, "[");

			da_foreach (Val, it, VLIST(v)) {
				char buf[1024]; val_sprint_f(*it, buf, depth + 1);
				sb_appendf(&sb, "%s", buf);
				if (it - VLIST(v)->items != VLIST(v)->count - 1)
					sb_appendf(&sb, ", ", buf);
			}

			sb_appendf(&sb, "]");
			sprintf(buf, "%s", sb.items);
			sb_free(&sb);
		} break;

		case VAL_DICT: {
			StringBuilder sb = {0};
			ValDict *dict = VDICT(v);
			size_t count = 0;

			sb_appendf(&sb, "{");
			ht_foreach_node (ValDict, dict, kv) {
				char buf[1024];
				val_sprint_f(kv->key, buf, depth + 1);
				sb_appendf(&sb, "%s: ", buf);
				val_sprint_f(kv->val, buf, depth + 1);
				sb_appendf(&sb, "%s", buf);
				if (count++ < dict->count - 1)
					sb_appendf(&sb, ", ");
			}

			sb_appendf(&sb, "}");
			sprintf(buf, "%s", sb.items);
			sb_free(&sb);
		} break;
	}
}

void val_sprint(Val v, char *buf) {
	val_sprint_f(v, buf, 0);
}

Val Int(EvalCtx *ctx, Location call_loc, Vals args) {
	if (args.count != 1)
		err(ctx, call_loc, "int() accepts only 1 argument");

	Val arg = args.items[0];
	switch (arg.kind) {
		case VAL_INT:
			return arg;

		case VAL_FLOAT:
			return (Val){
				.kind = VAL_INT,
				.as.vint = (long long) arg.as.vfloat
			};

		case VAL_STR: {
			char *end;
			return (Val){
				.kind = VAL_INT,
				.as.vint = strtoll(VSTR(arg)->items, &end, 10),
			};
		}

		default: err(ctx, call_loc, "cannot convert to int");
	}
}

Val Float(EvalCtx *ctx, Location call_loc, Vals args) {
	if (args.count != 1)
		err(ctx, call_loc, "float() accepts only 1 argument");

	Val arg = args.items[0];
	switch (arg.kind) {
		case VAL_FLOAT:
			return arg;

		case VAL_INT:
			return (Val){
				.kind = VAL_INT,
				.as.vfloat = (double) arg.as.vfloat
			};

		case VAL_STR: {
			char *end;
			return (Val){
				.kind = VAL_FLOAT,
				.as.vfloat = strtod(VSTR(arg)->items, &end),
			};
		}

		default: err(ctx, call_loc, "cannot convert to float");
	}
}

Val Len(EvalCtx *ctx, Location call_loc, Vals args) {
	if (args.count != 1)
		err(ctx, call_loc, "len() accepts only 1 argument");

	Val arg = args.items[0];
	long long len = 0;

	switch (arg.kind) {
		case VAL_LIST: len = VLIST(arg)->count; break;
		case VAL_DICT: len = VDICT(arg)->count; break;
		case VAL_STR:  len = VSTR(arg)->count;  break;
		default: err(ctx, call_loc, "len() accepts only lists, strings and dictionaries");
	}

	return (Val){
		.kind = VAL_INT,
		.as.vint = len,
	};
}

Val Str(EvalCtx *ctx, Location call_loc, Vals args) {
	Val str = eval_new_heap_val(ctx, VAL_STR);
	char buf[1 << 16];

	da_foreach (Val, v, &args) {
		val_sprint(*v, buf);
		sb_appendf(VSTR(str), "%s", buf);
	}

	return str;
}

Val Error(EvalCtx *ctx, Location call_loc, Vals args) {
	if (args.count != 1)
		err(ctx, call_loc, "error() accepts only 1 argument");
	if (args.items[0].kind != VAL_STR)
		err(ctx, call_loc, "error() accepts only string");

	err(ctx, call_loc, VSTR(args.items[0])->items);
	return VNONE;
}

Val Print(EvalCtx *ctx, Location call_loc, Vals args) {
	char buf[1 << 16];
	da_foreach (Val, it, &args) {
		val_sprint(*it, buf);
		printf("%s", buf);
	}

	return VNONE;
}

Val Println(EvalCtx *ctx, Location call_loc, Vals args) {
	Print(ctx, call_loc, args);
	printf("\n");
	return VNONE;
}

Val Input(EvalCtx *ctx, Location call_loc, Vals args) {
	if (args.items[0].kind != VAL_STR)
		err(ctx, call_loc, "input() accepts only string");

	char res[1024];
	Val str = eval_new_heap_val(ctx, VAL_STR);

	printf("%s", VSTR(args.items[0])->items);
	scanf("%s", res);
	sb_appendf(VSTR(str), "%s", res);

	return str;
}

Val Append(EvalCtx *ctx, Location call_loc, Vals args) {
	if (args.count < 2)
		err(ctx, call_loc, "append() accepts more than 1 arguments");

	if (args.items[0].kind == VAL_LIST) {
		Vals *list = VLIST(args.items[0]);
		for (size_t i = 1; i < args.count; i++)
			da_append(list, args.items[i]);
	} else if (args.items[0].kind == VAL_STR) {
		StringBuilder *str = VSTR(args.items[0]);
		for (size_t i = 1; i < args.count; i++) {
			if (args.items[i].kind != VAL_STR)
				err(ctx, call_loc, "append() accepts only strings for string appending");
			sb_appendf(str, "%s", VSTR(args.items[i])->items);
		}
	} else err(ctx, call_loc, "append() accepts only list or string");

	return VNONE;
}

Val Remove(EvalCtx *ctx, Location call_loc, Vals args) {
	if (args.count != 2)
		goto error;

	Val list = args.items[0];
	if (list.kind != VAL_LIST)
		goto error;

	Val ind = args.items[1];
	if (ind.kind != VAL_INT)
		goto error;

	da_remove_ordered(VLIST(list), ind.as.vint);
	return VNONE;

error:
	err(ctx, call_loc, "remove() accepts list and index");
	return VNONE;
}

Val Range(EvalCtx *ctx, Location call_loc, Vals args) {
	long long from = 0;
	long long to   = 0;
	long long step = 1;

	if (args.count < 1 || args.count > 3) {
		err(ctx, call_loc, "range() accepts 1, 2 and 3 arguments");
		return VNONE;
	}

	da_foreach (Val, val, &args) {
		if (val->kind != VAL_INT) {
			err(ctx, call_loc, "range() accepts only integers");
			return VNONE;
		}
	}

	if (args.count == 1) {
		to = args.items[0].as.vint;
	} else if (args.count == 2) {
		from = args.items[0].as.vint;
		to = args.items[1].as.vint;
	} else if (args.count == 3) {
		from = args.items[0].as.vint;
		to = args.items[1].as.vint;
		step = args.items[2].as.vint;
	}

	Val list = eval_new_heap_val(ctx, VAL_LIST);
	if (step > 0) {
		for (long long i = from; i < to; i += step)
			da_append(VLIST(list), ((Val){ .kind = VAL_INT, .as.vint = i }));
	} else {
		for (long long i = from; i > to; i += step)
			da_append(VLIST(list), ((Val){ .kind = VAL_INT, .as.vint = i }));
	}

	return list;
}

Val Insert(EvalCtx *ctx, Location call_loc, Vals args) {
	if (args.count != 3)
		goto error;

	if (args.items[0].kind != VAL_LIST)
		goto error;

	Vals *list = VLIST(args.items[0]);
	Val ind = args.items[1];
	if (ind.kind != VAL_INT)
		goto error;

	Val val = args.items[2];

	if (val.kind != VAL_INT) goto error;
	da_insert(list, ind.as.vint, val);
	return VNONE;

error:
	err(ctx, call_loc, "insert() accepts: list, index and value");
}

Val Kind(EvalCtx *ctx, Location call_loc, Vals args) {
	if (args.count != 1)
		err(ctx, call_loc, "kind() accepts only 1 argument");

	return (Val){
		.kind = VAL_INT,
		.as.vint = args.items[0].kind,
	};
}

Val Has(EvalCtx *ctx, Location call_loc, Vals args) {
	if (args.count != 2)
		goto error;

	bool res = false;

	if (args.items[0].kind == VAL_DICT) {
		ValDict *dict = VDICT(args.items[0]);
		Val key = args.items[1];
		res = ValDict_get(dict, key) != NULL;
	} else if (args.items[0].kind == VAL_LIST) {
		Vals *list = VLIST(args.items[0]);
		Val key = args.items[1];
		da_foreach (Val, v, list) {
			if (ValDict_compare(*v, key) == 0) {
				res = true;
				break;
			}
		}
	} else goto error;

	return (Val){
		.kind = VAL_BOOL,
		.as.vbool = res,
	};

error:
	err(ctx, call_loc, "has() accepts: dictionary or list, item");
	return VNONE;
}

void reg_kinds(EvalCtx *ctx) {
	eval_reg_var(ctx, "_VAL_NONE_",  (Val){.kind = VAL_INT, .as.vint = VAL_NONE});
	eval_reg_var(ctx, "_VAL_INT_",   (Val){.kind = VAL_INT, .as.vint = VAL_INT});
	eval_reg_var(ctx, "_VAL_BOOL_",  (Val){.kind = VAL_INT, .as.vint = VAL_BOOL});
	eval_reg_var(ctx, "_VAL_FLOAT_", (Val){.kind = VAL_INT, .as.vint = VAL_FLOAT});
	eval_reg_var(ctx, "_VAL_LIST_",  (Val){.kind = VAL_INT, .as.vint = VAL_LIST});
	eval_reg_var(ctx, "_VAL_DICT_",  (Val){.kind = VAL_INT, .as.vint = VAL_DICT});
	eval_reg_var(ctx, "_VAL_STR_",   (Val){.kind = VAL_INT, .as.vint = VAL_STR});
}

void reg_stdlib(EvalCtx *ctx) {
	reg_kinds(ctx);
	eval_reg_func(ctx, "len",     Len);
	eval_reg_func(ctx, "int",     Int);
	eval_reg_func(ctx, "float",   Float);
	eval_reg_func(ctx, "str",     Str);
	eval_reg_func(ctx, "print",   Print);
	eval_reg_func(ctx, "println", Println);
	eval_reg_func(ctx, "input",   Input);
	eval_reg_func(ctx, "range",   Range);
	eval_reg_func(ctx, "append",  Append);
	eval_reg_func(ctx, "has",     Has);
	eval_reg_func(ctx, "remove",  Remove);
	eval_reg_func(ctx, "insert",  Insert);
	eval_reg_func(ctx, "kind",    Kind);
	eval_reg_func(ctx, "error",   Error);
}
