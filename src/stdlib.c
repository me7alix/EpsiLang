#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../include/parser.h"
#include "../include/eval.h"

#define err(ec, loc, msg) \
do { \
	(ec)->got_err = true; \
	(ec)->errf(loc, ERROR_RUNTIME, msg); \
	return (Val){0}; \
} while(0)

void val_sprint(Val v, char *buf) {
	switch (v.kind) {
		case VAL_INT:
			sprintf(buf, "%lli", v.as.vint);
			break;

		case VAL_FLOAT:
			sprintf(buf, "%lf", v.as.vfloat);
			break;

		case VAL_BOOL:
			sprintf(buf, "%s", v.as.vbool ? "true" : "false");
			break;

		case VAL_STR:
			sprintf(buf, "%s", v.as.str->items);
			break;

		case VAL_LIST: {
			StringBuilder sb = {0};
			sb_appendf(&sb, "[");

			da_foreach (Val, it, v.as.list) {
				char buf[1024]; val_sprint(*it, buf);
				sb_appendf(&sb, "%s", buf);
				if (it - v.as.list->items != v.as.list->count - 1)
					sb_appendf(&sb, ", ", buf);
			}

			sb_appendf(&sb, "]");
			sprintf(buf, "%s", sb.items);
			sb_free(&sb);
		} break;

		case VAL_DICT: {
			StringBuilder sb = {0};
			ValDict *dict = v.as.dict;
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

Val Int(Location call_loc, ErrorCtx *ec, Vals args) {
	if (args.count != 1)
		err(ec, call_loc, "int() accepts only 1 argument");

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
				.as.vint = strtoll(arg.as.str->items, &end, 10),
			};
		}

		default: err(ec, call_loc, "cannot convert to int");
	}
}

Val Len(Location call_loc, ErrorCtx *ec, Vals args) {
	if (args.count != 1)
		err(ec, call_loc, "len() accepts only 1 argument");

	Val arg = args.items[0];
	long long len = 0;

	switch (arg.kind) {
		case VAL_LIST: len = arg.as.list->count;             break;
		case VAL_DICT: len = ((ValDict*)arg.as.dict)->count; break;
		case VAL_STR:  len = arg.as.str->count;              break;
		default: err(ec, call_loc, "len() accepts only lists, strings and dictionaries");
	}

	return (Val){
		.kind = VAL_INT,
		.as.vint = len,
	};
}

Val Str(Location call_loc, ErrorCtx *ec, Vals args) {
	if (args.count != 1)
		err(ec, call_loc, "str() accepts only 1 argument");

	StringBuilder *str = malloc(sizeof(*str));
	*str = (StringBuilder){0};

	char buf[1024];
	val_sprint(args.items[0], buf);
	sb_appendf(str, "%s", buf);

	return (Val){
		.kind = VAL_STR,
		.as.str = str,
	};
}

Val Error(Location call_loc, ErrorCtx *ec, Vals args) {
	if (args.count != 1)
		err(ec, call_loc, "error() accepts only 1 argument");
	if (args.items[0].kind != VAL_STR)
		err(ec, call_loc, "error() accepts only string");

	err(ec, call_loc, args.items[0].as.str->items);

	return (Val){0};
}

Val Print(Location call_loc, ErrorCtx *ec, Vals args) {
	char buf[1024];
	da_foreach (Val, it, &args) {
		val_sprint(*it, buf);
		printf("%s ", buf);
	}

	printf("\n");
	return (Val){0};
}

Val Input(Location call_loc, ErrorCtx *ec, Vals args) {
	if (args.items[0].kind != VAL_STR)
		err(ec, call_loc, "input() accepts only string");

	char res[1024];
	StringBuilder *str = malloc(sizeof(*str));
	*str = (StringBuilder){0};

	printf("%s", args.items[0].as.str->items);
	scanf("%s", res);
	sb_appendf(str, "%s", res);

	return (Val){
		.kind = VAL_STR,
		.as.str = str,
	};
}

Val Exit(Location call_loc, ErrorCtx *ec, Vals args) {
	if (args.count != 1)
		err(ec, call_loc, "exit() accepts only 1 argument");

	if (args.items[0].kind != VAL_INT)
		err(ec, call_loc, "exit() accepts only integer");

	exit(args.items[0].as.vint);
	return (Val){0};
}

Val System(Location call_loc, ErrorCtx *ec, Vals args) {
	if (args.count == 0)
		err(ec, call_loc, "arguments were not provided");

	StringBuilder str = {0};
	for (size_t i = 0; i < args.count; i++) {
		if (args.items[i].kind != VAL_STR)
			err(ec, call_loc, "system() accepts only strings");
		sb_appendf(&str, "%s ", args.items[i].as.str->items);
	}

	return (Val){
		.kind = VAL_INT,
		.as.vint = system(str.items),
	};
}

Val Append(Location call_loc, ErrorCtx *ec, Vals args) {
	if (args.count < 2)
		err(ec, call_loc, "append() accepts list or str as first argument");

	if (args.items[0].kind == VAL_LIST) {
		Vals *list = args.items[0].as.list;
		for (size_t i = 1; i < args.count; i++)
			da_append(list, args.items[i]);
	} else if (args.items[0].kind == VAL_STR) {
		StringBuilder *str = args.items[0].as.str;
		for (size_t i = 1; i < args.count; i++) {
			if (args.items[i].kind != VAL_STR)
				err(ec, call_loc, "append() accepts only strings for string appending");

			sb_appendf(str, "%s", args.items[i].as.str->items);
		}
	}

	return (Val){0};
}

Val Remove(Location call_loc, ErrorCtx *ec, Vals args) {
	if (args.count != 2)
		goto err;

	Val list = args.items[0];
	if (list.kind != VAL_LIST)
		goto err;

	Val ind = args.items[1];
	if (ind.kind != VAL_INT)
		goto err;

	da_remove_ordered(list.as.list, ind.as.vint);
	return (Val){0};

err:
	err(ec, call_loc, "remove() accepts list and index");
	return (Val){0};
}

Val Range(Location call_loc, ErrorCtx *ec, Vals args) {
	long long from = 0;
	long long to   = 0;
	long long step = 1;

	if (args.count < 1 || args.count > 3) {
		err(ec, call_loc, "range() accepts 1, 2 and 3 arguments");
		return (Val){0};
	}

	da_foreach (Val, val, &args) {
		if (val->kind != VAL_INT) {
			err(ec, call_loc, "range() accepts only integers");
			return (Val){0};
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

	Vals *list = malloc(sizeof(Vals));
	*list = (Vals){0};

	if (step > 0) {
		for (long long i = from; i < to; i += step)
			da_append(list, ((Val){ .kind = VAL_INT, .as.vint = i }));
	} else {
		for (long long i = from; i > to; i += step)
			da_append(list, ((Val){ .kind = VAL_INT, .as.vint = i }));
	}

	return (Val){
		.kind = VAL_LIST,
		.as.list = list,
	};
}

Val Insert(Location call_loc, ErrorCtx *ec, Vals args) {
	if (args.count != 3)
		goto err;

	if (args.items[0].kind != VAL_LIST)
		goto err;

	Vals *list = args.items[0].as.list;
	Val ind = args.items[1];
	if (ind.kind != VAL_INT)
		goto err;

	Val val = args.items[2];

	if (val.kind != VAL_INT) goto err;
	da_insert(list, ind.as.vint, val);
	return (Val){0};

err:
	err(ec, call_loc, "insert() accepts: list, index and value");
}

Val Has(Location call_loc, ErrorCtx *ec, Vals args) {
	if (args.count != 2)
		goto err;

	if (args.items[0].kind != VAL_DICT)
		goto err;

	ValDict *dict = args.items[0].as.dict;
	Val key = args.items[1];

	return (Val){
		.kind = VAL_BOOL,
		.as.vbool = ValDict_get(dict, key) != NULL,
	};

err:
	err(ec, call_loc, "has() accepts: dictionary, item");
	return (Val){0};
}


void reg_platform(Parser *p, EvalCtx *ctx) {
	StringBuilder *str = malloc(sizeof(*str));
	*str = (StringBuilder){0};
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

	sb_appendf(str, "%s", platform);
	reg_var(ctx, "_OS_", (Val){
		.kind = VAL_STR,
		.as.str = str,
	});
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
