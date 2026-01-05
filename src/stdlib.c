#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../include/parser.h"
#include "../include/eval.h"

#define bstr(b) (b ? "true" : "false")

#define err(ec, loc, msg) \
do { \
	(ec)->got_err = true; \
	(ec)->errf(loc, ERROR_RUNTIME, msg); \
	return NULL_VAL; \
} while(0)

void val_sprint(Val v, char *buf) {
	switch (v.kind) {
		case VAL_INT:   sprintf(buf, "%lli", v.as.vint);      break;
		case VAL_FLOAT: sprintf(buf, "%lf", v.as.vfloat);     break;
		case VAL_BOOL:  sprintf(buf, "%s", bstr(v.as.vbool)); break;

		case VAL_STR:
			sprintf(buf, "%s", VSTR(v)->items);
			break;

		case VAL_LIST: {
			StringBuilder sb = {0};
			sb_appendf(&sb, "[");

			da_foreach (Val, it, VLIST(v)) {
				char buf[1024]; val_sprint(*it, buf);
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
				val_sprint(kv->key, buf);
				sb_appendf(&sb, "%s: ", buf);
				val_sprint(kv->val, buf);
				sb_appendf(&sb, "%s", buf);
				if (count++ < dict->count - 1)
					sb_appendf(&sb, ", ");
			}

			sb_appendf(&sb, "}");
			sprintf(buf, "%s", sb.items);
			sb_free(&sb);
		} break;

		default:
			sprintf(buf, "err");
			break;
	}
}

Val Int(EvalCtx *ctx, Location call_loc, Vals args) {
	if (args.count != 1)
		err(&ctx->err, call_loc, "int() accepts only 1 argument");

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

		default: err(&ctx->err, call_loc, "cannot convert to int");
	}
}


Val Len(EvalCtx *ctx, Location call_loc, Vals args) {
	if (args.count != 1)
		err(&ctx->err, call_loc, "len() accepts only 1 argument");

	Val arg = args.items[0];
	long long len = 0;

	switch (arg.kind) {
		case VAL_LIST: len = VLIST(arg)->count; break;
		case VAL_DICT: len = VDICT(arg)->count; break;
		case VAL_STR:  len = VSTR(arg)->count;  break;
		default: err(&ctx->err, call_loc, "len() accepts only lists, strings and dictionaries");
	}

	return (Val){
		.kind = VAL_INT,
		.as.vint = len,
	};
}

Val Str(EvalCtx *ctx, Location call_loc, Vals args) {
	if (args.count != 1)
		err(&ctx->err, call_loc, "str() accepts only 1 argument");

	Val str = eval_new_heap_val(ctx, VAL_STR);

	char buf[1024];
	val_sprint(args.items[0], buf);
	sb_appendf(VSTR(str), "%s", buf);

	return str;
}

Val Error(EvalCtx *ctx, Location call_loc, Vals args) {
	if (args.count != 1)
		err(&ctx->err, call_loc, "error() accepts only 1 argument");
	if (args.items[0].kind != VAL_STR)
		err(&ctx->err, call_loc, "error() accepts only string");

	err(&ctx->err, call_loc, VSTR(args.items[0])->items);
	return NULL_VAL;
}

Val Print(EvalCtx *ctx, Location call_loc, Vals args) {
	char buf[1024];
	da_foreach (Val, it, &args) {
		val_sprint(*it, buf);
		printf("%s ", buf);
	}

	printf("\n");
	return NULL_VAL;
}

Val Input(EvalCtx *ctx, Location call_loc, Vals args) {
	if (args.items[0].kind != VAL_STR)
		err(&ctx->err, call_loc, "input() accepts only string");

	char res[1024];
	Val str = eval_new_heap_val(ctx, VAL_STR);

	printf("%s", VSTR(args.items[0])->items);
	scanf("%s", res);
	sb_appendf(VSTR(str), "%s", res);

	return str;
}

Val Exit(EvalCtx *ctx, Location call_loc, Vals args) {
	if (args.count != 1)
		err(&ctx->err, call_loc, "exit() accepts only 1 argument");

	if (args.items[0].kind != VAL_INT)
		err(&ctx->err, call_loc, "exit() accepts only integer");

	exit(args.items[0].as.vint);
	return NULL_VAL;
}

Val System(EvalCtx *ctx, Location call_loc, Vals args) {
	if (args.count == 0)
		err(&ctx->err, call_loc, "arguments were not provided");

	StringBuilder str = {0};
	for (size_t i = 0; i < args.count; i++) {
		if (args.items[i].kind != VAL_STR)
			err(&ctx->err, call_loc, "system() accepts only strings");
		sb_appendf(&str, "%s ", VSTR(args.items[i])->items);
	}

	int res = system(str.items);
	sb_free(&str);

	return (Val){
		.kind = VAL_INT,
		.as.vint = res,
	};
}

Val Append(EvalCtx *ctx, Location call_loc, Vals args) {
	if (args.count < 2)
		err(&ctx->err, call_loc, "append() accepts more than 1 arguments");

	if (args.items[0].kind == VAL_LIST) {
		Vals *list = VLIST(args.items[0]);
		for (size_t i = 1; i < args.count; i++)
			da_append(list, args.items[i]);
	} else if (args.items[0].kind == VAL_STR) {
		StringBuilder *str = VSTR(args.items[0]);
		for (size_t i = 1; i < args.count; i++) {
			if (args.items[i].kind != VAL_STR)
				err(&ctx->err, call_loc, "append() accepts only strings for string appending");
			sb_appendf(str, "%s", VSTR(args.items[i])->items);
		}
	} else err(&ctx->err, call_loc, "append() accepts only list or string");

	return NULL_VAL;
}

Val Remove(EvalCtx *ctx, Location call_loc, Vals args) {
	if (args.count != 2)
		goto err;

	Val list = args.items[0];
	if (list.kind != VAL_LIST)
		goto err;

	Val ind = args.items[1];
	if (ind.kind != VAL_INT)
		goto err;

	da_remove_ordered(VLIST(list), ind.as.vint);
	return NULL_VAL;

err:
	err(&ctx->err, call_loc, "remove() accepts list and index");
	return NULL_VAL;
}

Val Range(EvalCtx *ctx, Location call_loc, Vals args) {
	long long from = 0;
	long long to   = 0;
	long long step = 1;

	if (args.count < 1 || args.count > 3) {
		err(&ctx->err, call_loc, "range() accepts 1, 2 and 3 arguments");
		return NULL_VAL;
	}

	da_foreach (Val, val, &args) {
		if (val->kind != VAL_INT) {
			err(&ctx->err, call_loc, "range() accepts only integers");
			return NULL_VAL;
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
		goto err;

	if (args.items[0].kind != VAL_LIST)
		goto err;

	Vals *list = VLIST(args.items[0]);
	Val ind = args.items[1];
	if (ind.kind != VAL_INT)
		goto err;

	Val val = args.items[2];

	if (val.kind != VAL_INT) goto err;
	da_insert(list, ind.as.vint, val);
	return NULL_VAL;

err:
	err(&ctx->err, call_loc, "insert() accepts: list, index and value");
}

Val Has(EvalCtx *ctx, Location call_loc, Vals args) {
	if (args.count != 2)
		goto err;

	if (args.items[0].kind != VAL_DICT)
		goto err;

	ValDict *dict = VDICT(args.items[0]);
	Val key = args.items[1];

	return (Val){
		.kind = VAL_BOOL,
		.as.vbool = ValDict_get(dict, key) != NULL,
	};

err:
	err(&ctx->err, call_loc, "has() accepts: dictionary, item");
	return NULL_VAL;
}


void reg_platform(Parser *p, EvalCtx *ctx) {
	Val str = eval_new_heap_val(ctx, VAL_STR);

	char *platform;
#if defined(_WIN32)
	platform = "WINDOWS";
#elif defined(__linux__)
	platform = "LINUX";
#elif defined(__APPLE__)
	platform = "APPLE";
#else
	platform = "NONE";
#endif

	sb_appendf(VSTR(str), "%s", platform);
	reg_var(ctx, "_OS_", str);
}

void reg_stdlib(Parser *p, EvalCtx *ctx) {
	reg_platform(p, ctx);
	reg_func(ctx, "len",    Len);
	reg_func(ctx, "int",    Int);
	reg_func(ctx, "str",    Str);
	reg_func(ctx, "print",  Print);
	reg_func(ctx, "input",  Input);
	reg_func(ctx, "range",  Range);
	reg_func(ctx, "append", Append);
	reg_func(ctx, "has",    Has);
	reg_func(ctx, "remove", Remove);
	reg_func(ctx, "insert", Insert);
	reg_func(ctx, "exit",   Exit);
	reg_func(ctx, "system", System);
	reg_func(ctx, "error",  Error);
}
