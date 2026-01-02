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

		default:
			sprintf(buf, "err");
			break;
	}
}

Val Print(Vals args) {
	char buf[256];
	da_foreach (Val, it, &args) {
		val_sprint(*it, buf);
		printf("%s ", buf);
	}

	printf("\n");
	return (Val){0};
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
	Vals *list = args.items[0].as.list;
	for (size_t i = 1; i < args.count; i++)
		da_append(list, args.items[i]);
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

void reg_stdlib(Parser *p, EvalCtx *ctx) {
	reg_func(p, ctx, Print,  "print",  FAC_GREAT, 0);
	reg_func(p, ctx, Append, "append", FAC_GREAT, 1);
	reg_func(p, ctx, Remove, "remove", FAC_EQ,    1);
	reg_func(p, ctx, Insert, "insert", FAC_EQ,    3);
	reg_func(p, ctx, Sqrt,   "sqrt",   FAC_EQ,    1);
	reg_func(p, ctx, Exit,   "exit",   FAC_EQ,    1);
}
