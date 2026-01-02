#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "../include/parser.h"
#include "../include/eval.h"

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
			ValDict* dict = v.as.dict;
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

Val Int(Vals args) {
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

		default:
			assert(!"cannot convert to int");
	}
}

Val Str(Vals args) {
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

Val Print(Vals args) {
	char buf[1024];
	da_foreach (Val, it, &args) {
		val_sprint(*it, buf);
		printf("%s ", buf);
	}

	printf("\n");
	return (Val){0};
}

Val Input(Vals args) {
	StringBuilder *str = malloc(sizeof(*str));
	*str = (StringBuilder){0};

	char res[1024];
	printf("%s", args.items[0].as.str->items);
	scanf("%s", res);
	sb_appendf(str, "%s", res);

	return (Val){
		.kind = VAL_STR,
		.as.str = str,
	};
}

Val Sqrt(Vals args) {
	Val arg = da_get(&args, 0);
	double val =
		arg.kind == VAL_FLOAT ? arg.as.vfloat :
		arg.kind == VAL_INT   ? arg.as.vint   :
		(assert(!"sqrt accepts: float or integer"), 0);

	return (Val){
		.kind = VAL_FLOAT,
		.as.vfloat = sqrt(val)
	};
}

Val Exit(Vals args) {
	Val arg = da_get(&args, 0);
	long long val =
		arg.kind == VAL_INT ? arg.as.vint :
		(assert(!"integer expected"), 0);

	exit(val);
	return (Val){0};
}

Val Append(Vals args) {
	if (args.items[0].kind == VAL_LIST) {
		Vals *list = args.items[0].as.list;
		for (size_t i = 1; i < args.count; i++)
			da_append(list, args.items[i]);
	} else if (args.items[0].kind == VAL_STR) {
		StringBuilder *str = args.items[0].as.str;
		for (size_t i = 1; i < args.count; i++) {
			if (args.items[i].kind != VAL_STR) assert("support only str for appending for now");
			sb_appendf(str, "%s%s", str->items, args.items[i].as.str);
		}
	}

	return (Val){0};
}

Val Remove(Vals args) {
	Vals *list = args.items[0].as.list;
	Val ind = args.items[1];
	if (ind.kind != VAL_INT) goto err;
	da_remove_ordered(list, ind.as.vint);
	return (Val){0};

err:
	assert(!"remove accepts: list as 1st arg and index as 2nd arg");
	return (Val){0};
}

Val Insert(Vals args) {
	if (args.items[0].kind != VAL_LIST)
		goto err;

	Vals *list = args.items[0].as.list;
	Val ind = args.items[1];
	Val val = args.items[2];

	if (val.kind != VAL_INT) goto err;
	da_insert(list, ind.as.vint, val);
	return (Val){0};

err:
	assert(!"insert accepts: list as 1st arg, index as 2nd arg and value as 3rd arg");
	return (Val){0};
}

Val Has(Vals args) {
	if (args.items[0].kind != VAL_DICT)
		goto err;

	ValDict *dict = args.items[0].as.dict;
	Val key = args.items[1];

	return (Val){
		.kind = VAL_BOOL,
		.as.vbool = ValDict_get(dict, key) != NULL,
	};

err:
	assert(!"insert accepts: list as 1st arg, index as 2nd arg and value as 3rd arg");
	return (Val){0};
}

void reg_stdlib(Parser *p, EvalCtx *ctx) {
	reg_func(p, ctx, Has,    "has",    FAC_EQ,    2);
	reg_func(p, ctx, Int,    "int",    FAC_EQ,    1);
	reg_func(p, ctx, Str,    "str",    FAC_EQ,    1);
	reg_func(p, ctx, Print,  "print",  FAC_GREAT, 0);
	reg_func(p, ctx, Input,  "input",  FAC_EQ,    1);
	reg_func(p, ctx, Append, "append", FAC_GREAT, 1);
	reg_func(p, ctx, Remove, "remove", FAC_EQ,    1);
	reg_func(p, ctx, Insert, "insert", FAC_EQ,    3);
	reg_func(p, ctx, Sqrt,   "sqrt",   FAC_EQ,    1);
	reg_func(p, ctx, Exit,   "exit",   FAC_EQ,    1);
}
