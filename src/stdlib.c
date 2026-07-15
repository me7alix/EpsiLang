#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../include/eval.h"

#define err(ec, loc, msg) \
	do { \
		(ec)->err_ctx.got_err = true; \
		(ec)->err_ctx.errf(&(ec)->call_stack, loc, ERROR_RUNTIME, msg); \
		return VNONE; \
	} while(0)

void val_sprint(Val v, StringBuilder *sb, int depth) {
	switch (v.kind) {
	case VAL_NONE:  sb_appendf(sb, "none");                 break;
	case VAL_INT:   sb_appendf(sb, "%lli", v.as.vint);      break;
	case VAL_FLOAT: sb_appendf(sb, "%lf", v.as.vfloat);     break;

	case VAL_BOOL:
		sb_appendf(sb, "%s", v.as.vbool ? "true" : "false");
		break;

	case VAL_CUSTOM:
		sb_appendf(sb, "CUSTOM(%d)", ((EvalCustomObj*)(v.as.gc_obj->data))->kind);
		break;

	case VAL_RUNE:
		char out[4];
		int n = utf8_encode(v.as.vrune, out);
		if (depth == 0) {
			sb_appendf(sb, "%.*s", n, out);
		} else {
			sb_appendf(sb, "'%.*s'", n, out);
		} break;

	case VAL_FIELD:
		if (depth == 0) {
			sb_appendf(sb, "%s", v.as.field);
		} else {
			sb_appendf(sb, ".%s", v.as.field);
		} break;

	case VAL_STR:
		if (depth == 0) {
			sb_appendf(sb, "%s", VSTR(v)->items);
		} else {
			sb_appendf(sb, "\"%s\"", VSTR(v)->items);
		} break;

	case VAL_LIST: {
		sb_appendf(sb, "[");
		da_foreach (Val, it, VLIST(v)) {
			val_sprint(*it, sb, depth + 1);
			if (it - VLIST(v)->items != VLIST(v)->count - 1) {
				sb_appendf(sb, ", ");
			}
		}
		sb_appendf(sb, "]");
	} break;

	case VAL_DICT:
		ValDict *dict = VDICT(v);
		size_t count = 0;
		sb_appendf(sb, "{");
		ht_foreach_node (ValDict, dict, kv) {
			val_sprint(kv->key, sb, depth + 1);
			sb_appendf(sb, ": ");
			val_sprint(kv->val, sb, depth + 1);
			if (count++ < dict->count - 1) {
				sb_appendf(sb, ", ");
			}
		}
		sb_appendf(sb, "}");
	}
}

void JsonSerializeF(
	StringBuilder *sb,
	Val v,
	long long intend,
	long long spaces
) {
#define ADD_INTENDATION() \
	do { if (spaces != 0) sb_appendf(sb, "%*s", intend, ""); } while(0)

	switch (v.kind) {
	case VAL_NONE:  sb_appendf(sb, "null");                   break;
	case VAL_INT:   sb_appendf(sb, "%lli", v.as.vint);        break;
	case VAL_FLOAT: sb_appendf(sb, "%lf", v.as.vfloat);       break;
	case VAL_STR:   sb_appendf(sb, "\"%s\"", VSTR(v)->items); break;

	case VAL_BOOL:
		sb_appendf(sb, "%s", v.as.vbool ? "true" : "false");
		break;

	case VAL_LIST: {
		intend += spaces;
		sb_appendf(sb, "[");
		if (spaces != 0)
			sb_appendf(sb, "\n");

		da_foreach (Val, it, VLIST(v)) {
			ADD_INTENDATION();
			JsonSerializeF(sb, *it, intend, spaces);
			if (it - VLIST(v)->items != VLIST(v)->count - 1) {
				if (spaces == 0) sb_appendf(sb, ", ");
				else             sb_appendf(sb, ",\n");
			}
		}

		if (spaces != 0)
			sb_appendf(sb, "\n");

		intend -= spaces;
		ADD_INTENDATION();
		sb_appendf(sb, "]");
	} break;

	case VAL_DICT: {
		ValDict *dict = VDICT(v);
		size_t count = 0;

		intend += spaces;
		sb_appendf(sb, "{");
		if (spaces != 0)
			sb_appendf(sb, "\n");

		ht_foreach_node (ValDict, dict, kv) {
			ADD_INTENDATION();
			JsonSerializeF(sb, kv->key, intend, spaces);
			sb_appendf(sb, ": ");
			JsonSerializeF(sb, kv->val, intend, spaces);
			if (count++ < dict->count - 1) {
				if (spaces == 0) sb_appendf(sb, ", ");
				else             sb_appendf(sb, ",\n");
			}
		}

		if (spaces != 0)
			sb_appendf(sb, "\n");

		intend -= spaces;
		ADD_INTENDATION();
		sb_appendf(sb, "}");
		} break;
	}
#undef ADD_INTENDATION
}

Val JsonSerialize(EvalCtx *ctx, Location cloc, Vals args) {
	if (args.count < 1 || args.count > 2)
		err(ctx, cloc, "accepts only 1 or 2 arguments");

	bool two_args = args.count == 2;
	if (two_args == 2 && args.items[1].kind != VAL_INT)
		err(ctx, cloc, "int expected as second argument");

	long long spaces = 0;
	if (two_args) spaces = args.items[1].as.vint;

	Val str = eval_make_val(ctx, VAL_STR);
	JsonSerializeF(VSTR(str), args.items[0], 0, spaces);
	return str;
}

typedef struct {
	EvalCtx *ectx;
	Lexer *lex;
	bool got_err;
} JsonParserCtx;

Val json_obj(JsonParserCtx *ctx, Val res) {
	Val obj = eval_make_val(ctx->ectx, VAL_DICT);
	Val res_fld = {.kind = VAL_FIELD, .as.field = "res"};
	Val err_fld = {.kind = VAL_FIELD, .as.field = "err"};
	ValDict_add(VDICT(obj), res_fld, res);
	ValDict_add(VDICT(obj), err_fld, VNONE);
	return obj;
}

Val json_err(JsonParserCtx *ctx, char *msg) {
	ctx->got_err = true;

	Val err     = eval_make_val(ctx->ectx, VAL_DICT);
	Val res_fld = {.kind = VAL_FIELD, .as.field = "res"};
	Val err_fld = {.kind = VAL_FIELD, .as.field = "err"};
	Val err_msg = eval_make_val(ctx->ectx, VAL_STR);

	size_t lines = ctx->lex->cur_loc.line_num + 1;
	size_t chars = ctx->lex->cur_loc.line_char - ctx->lex->cur_loc.line_start + 1;
	sb_appendf(VSTR(err_msg), "%zu:%zu: %s", lines, chars, msg);

	ValDict_add(VDICT(err), res_fld, VNONE);
	ValDict_add(VDICT(err), err_fld, err_msg);
	return err;
}

Val parse_json(JsonParserCtx *ctx) {
	switch (lexer_peek(ctx->lex).kind) {
	case TOK_ID: {
		char *id = lexer_peek(ctx->lex).data;
		if (strcmp(id, "null") == 0) {
			lexer_next(ctx->lex);
			return (Val){.kind = VAL_NONE};
		}
	} break;

	case TOK_TRUE:
		lexer_next(ctx->lex);
		return (Val){.kind = VAL_BOOL, .as.vbool = true};

	case TOK_FALSE:
		lexer_next(ctx->lex);
		return (Val){.kind = VAL_BOOL, .as.vbool = false};

	case TOK_INT: {
		char *end;
		return (Val){
			.kind = VAL_INT,
			.as.vint = strtoll(lexer_next(ctx->lex).data, &end, 0),
		};
	}

	case TOK_FLOAT:
		return (Val){
			.kind = VAL_FLOAT,
			.as.vfloat = atof(lexer_next(ctx->lex).data),
		};

	case TOK_OBRA: {
		lexer_next(ctx->lex);
		Val obj = eval_make_val(ctx->ectx, VAL_DICT);
		while (lexer_peek(ctx->lex).kind != TOK_CBRA) {
			Val key = parse_json(ctx);
			if (ctx->got_err) return key;

			if (key.kind != VAL_STR)
				return json_err(ctx, "string literal expected");

			if (lexer_peek(ctx->lex).kind != TOK_COL)
				return json_err(ctx, "column expected");
			lexer_next(ctx->lex);

			Val val = parse_json(ctx);
			if (ctx->got_err) return val;

			ValDict_add(VDICT(obj), key, val);

			if (lexer_peek(ctx->lex).kind == TOK_COM) {
				if (lexer_peek2(ctx->lex).kind == TOK_CBRA)
					return json_err(ctx, "deprived comma");
				lexer_next(ctx->lex);
			} else if (lexer_peek(ctx->lex).kind != TOK_CBRA)
				return json_err(ctx, "comma expected");
		}
		lexer_next(ctx->lex);
		return obj;
	}

	case TOK_OSQBRA: {
		lexer_next(ctx->lex);
		Val list = eval_make_val(ctx->ectx, VAL_LIST);
		while (lexer_peek(ctx->lex).kind != TOK_CSQBRA) {
			Val val = parse_json(ctx);
			if (ctx->got_err) return val;

			da_append(VLIST(list), val);

			if (lexer_peek(ctx->lex).kind == TOK_COM) {
				if (lexer_peek2(ctx->lex).kind == TOK_CSQBRA)
					return json_err(ctx, "deprived comma");
				lexer_next(ctx->lex);
			} else if (lexer_peek(ctx->lex).kind != TOK_CSQBRA)
				return json_err(ctx, "comma expected");
		}
		lexer_next(ctx->lex);
		return list;
	}

	case TOK_STRING:
		Val str = eval_make_val(ctx->ectx, VAL_STR);
		sb_appendf(VSTR(str), "%s", lexer_next(ctx->lex).data);
		return str;
	}

	return json_err(ctx, "unexpected token");
}

Val JsonDeserialize(EvalCtx *ctx, Location cloc, Vals args) {
	if (args.count != 1)
		err(ctx, cloc, "accepts only 1 argument");

	if (args.items[0].kind != VAL_STR)
		err(ctx, cloc, "expected string as first argument");

	Val str = args.items[0];
	Arena arena = {0};
	Lexer lexer = lexer_from_str(&arena, "json", VSTR(str)->items);

	JsonParserCtx jctx = {
		.ectx = ctx,
		.lex = &lexer,
		.got_err = false,
	};

	Val res = parse_json(&jctx);
	arena_free(&arena);
	lexer_free(&lexer);
	if (jctx.got_err) return res;
	return json_obj(&jctx, res);
}

bool get_number(Val val, double *num) {
	switch (val.kind) {
	case VAL_FLOAT:
		*num = val.as.vfloat;
		return true;
	case VAL_INT:
		*num = val.as.vint;
		return true;
	default:
		return false;
	}
}

Val BitXor(EvalCtx *ctx, Location cloc, Vals args) {
	if (args.count != 2)
		err(ctx, cloc, "accepts only 2 argument");

	if (args.items[0].kind != VAL_INT || args.items[1].kind != VAL_INT)
		err(ctx, cloc, "accepts only integers");

	return (Val){
		.kind = VAL_INT,
		.as.vint = args.items[0].as.vint ^ args.items[1].as.vint,
	};
}

Val Sin(EvalCtx *ctx, Location cloc, Vals args) {
	if (args.count != 1)
		err(ctx, cloc, "accepts only 1 argument");
	double number;
	if (!get_number(args.items[0], &number))
		err(ctx, cloc, "accepts only number");
	return (Val){
		.kind = VAL_FLOAT,
		.as.vfloat = sin(number),
	};
}

Val Cos(EvalCtx *ctx, Location cloc, Vals args) {
	if (args.count != 1)
		err(ctx, cloc, "accepts only 1 argument");
	double number;
	if (!get_number(args.items[0], &number))
		err(ctx, cloc, "accepts only number");
	return (Val){
		.kind = VAL_FLOAT,
		.as.vfloat = cos(number),
	};
}

Val Sqrt(EvalCtx *ctx, Location cloc, Vals args) {
	if (args.count != 1)
		err(ctx, cloc, "accepts only 1 argument");
	double number;
	if (!get_number(args.items[0], &number))
		err(ctx, cloc, "accepts only number");
	return (Val){
		.kind = VAL_FLOAT,
		.as.vfloat = sqrt(number),
	};
}

Val Pow(EvalCtx *ctx, Location cloc, Vals args) {
	if (args.count != 2)
		err(ctx, cloc, "accepts only 2 argument");
	double number1, number2;
	if (!get_number(args.items[0], &number1) || !get_number(args.items[1], &number2))
		err(ctx, cloc, "accepts only numbers");
	return (Val){
		.kind = VAL_FLOAT,
		.as.vfloat = pow(number1, number2),
	};
}

Val Floor(EvalCtx *ctx, Location cloc, Vals args) {
	if (args.count != 1)
		err(ctx, cloc, "accepts only 1 argument");

	if (args.items[0].kind != VAL_FLOAT)
		err(ctx, cloc, "accepts only float");

	return (Val){
		.kind = VAL_INT,
		.as.vint = floor(args.items[0].as.vfloat),
	};
}

Val Ceil(EvalCtx *ctx, Location cloc, Vals args) {
	if (args.count != 1)
		err(ctx, cloc, "accepts only 1 argument");

	if (args.items[0].kind != VAL_FLOAT)
		err(ctx, cloc, "accepts only float");

	return (Val){
		.kind = VAL_INT,
		.as.vint = ceil(args.items[0].as.vfloat),
	};
}

Val Max(EvalCtx *ctx, Location cloc, Vals args) {
	if (args.count == 2) {
		if (args.items[0].kind == VAL_INT && args.items[1].kind == VAL_INT) {
			return (Val){
				.kind = VAL_INT,
				.as.vint = args.items[0].as.vint > args.items[1].as.vint ?
					args.items[0].as.vint : args.items[1].as.vint,
			};
		} if (args.items[0].kind == VAL_INT && args.items[1].kind == VAL_FLOAT) {
			return (Val){
				.kind = VAL_FLOAT,
				.as.vfloat = args.items[0].as.vint > args.items[1].as.vfloat ?
					args.items[0].as.vint : args.items[1].as.vfloat,
			};
		} if (args.items[0].kind == VAL_FLOAT && args.items[1].kind == VAL_INT) {
			return (Val){
				.kind = VAL_FLOAT,
				.as.vfloat = args.items[0].as.vfloat > args.items[1].as.vint ?
					args.items[0].as.vfloat : args.items[1].as.vint,
			};
		} if (args.items[0].kind == VAL_FLOAT && args.items[1].kind == VAL_FLOAT) {
			return (Val){
				.kind = VAL_FLOAT,
				.as.vfloat = args.items[0].as.vfloat > args.items[1].as.vfloat ?
					args.items[0].as.vfloat : args.items[1].as.vfloat,
			};
		}
	}
	err(ctx, cloc, "accepts 2 numbers");
}

Val Min(EvalCtx *ctx, Location cloc, Vals args) {
	if (args.count == 2) {
		if (args.items[0].kind == VAL_INT && args.items[1].kind == VAL_INT) {
			return (Val){
				.kind = VAL_INT,
				.as.vint = args.items[0].as.vint < args.items[1].as.vint ?
					args.items[0].as.vint : args.items[1].as.vint,
			};
		} if (args.items[0].kind == VAL_INT && args.items[1].kind == VAL_FLOAT) {
			return (Val){
				.kind = VAL_FLOAT,
				.as.vfloat = args.items[0].as.vint < args.items[1].as.vfloat ?
					args.items[0].as.vint : args.items[1].as.vfloat,
			};
		} if (args.items[0].kind == VAL_FLOAT && args.items[1].kind == VAL_INT) {
			return (Val){
				.kind = VAL_FLOAT,
				.as.vfloat = args.items[0].as.vfloat < args.items[1].as.vint ?
					args.items[0].as.vfloat : args.items[1].as.vint,
			};
		} if (args.items[0].kind == VAL_FLOAT && args.items[1].kind == VAL_FLOAT) {
			return (Val){
				.kind = VAL_FLOAT,
				.as.vfloat = args.items[0].as.vfloat < args.items[1].as.vfloat ?
					args.items[0].as.vfloat : args.items[1].as.vfloat,
			};
		}
	}
	err(ctx, cloc, "accepts 2 numbers");
}

Val Split(EvalCtx *ctx, Location cloc, Vals args) {
	if (args.count != 2)
		err(ctx, cloc, "accepts only 2 arguments");

	if (args.items[0].kind != VAL_STR || args.items[1].kind != VAL_STR)
		err(ctx, cloc, "accepts only strings");

	Val str = args.items[0];
	Val del = args.items[1];
	Val arr = eval_make_val(ctx, VAL_LIST);

	StringBuilder cstr = {0};
	sb_appendf(&cstr, "%s", VSTR(str)->items);

	char *pch = strtok(cstr.items, VSTR(del)->items);
	while (pch) {
		Val nstr = eval_make_val(ctx, VAL_STR);
		sb_appendf(VSTR(nstr), "%s", pch);
		da_append(VLIST(arr), nstr);
		pch = strtok(NULL, VSTR(del)->items);
	}

	sb_free(&cstr);
	return arr;
}

Val Int(EvalCtx *ctx, Location cloc, Vals args) {
	if (args.count != 1)
		err(ctx, cloc, "accepts only 1 argument");

	Val arg = args.items[0];
	switch (arg.kind) {
		case VAL_INT:
			return arg;

		case VAL_RUNE:
			return (Val){
				.kind = VAL_INT,
				.as.vint = (long long) arg.as.vrune
			};

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

		default: err(ctx, cloc, "cannot convert to int");
	}
}

Val Float(EvalCtx *ctx, Location cloc, Vals args) {
	if (args.count != 1) {
		err(ctx, cloc, "accepts only 1 argument");
	}

	Val arg = args.items[0];
	double res;
	char *end;

	switch (arg.kind) {
	case VAL_FLOAT: res = arg.as.vfloat;                  break;
	case VAL_INT:   res = arg.as.vint;                    break;
	case VAL_STR:   res = strtod(VSTR(arg)->items, &end); break;
	default:        err(ctx, cloc, "cannot convert to float");
	}

	return (Val){
		.kind = VAL_FLOAT,
		.as.vfloat = res,
	};
}

Val Rune(EvalCtx *ctx, Location cloc, Vals args) {
	if (args.count != 1) {
		err(ctx, cloc, "accepts only 1 argument");
	}

	Val arg = args.items[0];
	UTF8_Rune res;

	switch (arg.kind) {
	case VAL_RUNE: res = arg.as.vrune; break;
	case VAL_INT:  res = arg.as.vint;  break;
	default:       err(ctx, cloc, "cannot convert to rune");
	}

	return (Val){
		.kind = VAL_RUNE,
		.as.vrune = res,
	};
}

Val Len(EvalCtx *ctx, Location cloc, Vals args) {
	if (args.count != 1)
		err(ctx, cloc, "accepts only 1 argument");

	Val arg = args.items[0];
	long long len = 0;

	switch (arg.kind) {
	case VAL_LIST: len = VLIST(arg)->count;          break;
	case VAL_DICT: len = VDICT(arg)->count;          break;
	case VAL_STR:  len = utf8_len(VSTR(arg)->items); break;
	default: err(ctx, cloc, "accepts only lists, strings and dictionaries");
	}

	return (Val){
		.kind = VAL_INT,
		.as.vint = len,
	};
}

Val Str(EvalCtx *ctx, Location cloc, Vals args) {
	Val str = eval_make_val(ctx, VAL_STR);

	da_foreach (Val, v, &args) {
		val_sprint(*v, VSTR(str), 0);
	}

	return str;
}

Val Error(EvalCtx *ctx, Location cloc, Vals args) {
	if (args.count != 1)
		err(ctx, cloc, "accepts only 1 argument");
	if (args.items[0].kind != VAL_STR)
		err(ctx, cloc, "accepts only string");

	err(ctx, cloc, VSTR(args.items[0])->items);
	return VNONE;
}

Val Print(EvalCtx *ctx, Location cloc, Vals args) {
	StringBuilder sb = {0};

	da_foreach (Val, it, &args) {
		val_sprint(*it, &sb, 0);
	}

	printf("%s", sb.items);
	da_free(&sb);

	return VNONE;
}

Val Println(EvalCtx *ctx, Location cloc, Vals args) {
	Print(ctx, cloc, args);
	printf("\n");
	return VNONE;
}

Val Input(EvalCtx *ctx, Location cloc, Vals args) {
	if (args.items[0].kind != VAL_STR)
		err(ctx, cloc, "accepts only string");

	char res[1 << 12];
	Val str = eval_make_val(ctx, VAL_STR);

	printf("%s", VSTR(args.items[0])->items);
	if (fgets(res, sizeof res, stdin) == NULL)
		err(ctx, cloc, "no input");

	res[strlen(res) - 1] = '\0';
	sb_appendf(VSTR(str), "%s", res);
	return str;
}

Val Append(EvalCtx *ctx, Location cloc, Vals args) {
	if (args.count < 2)
		err(ctx, cloc, "accepts more than 1 arguments");

	if (args.items[0].kind == VAL_LIST) {
		Vals *list = VLIST(args.items[0]);
		for (size_t i = 1; i < args.count; i++)
			da_append(list, args.items[i]);
	} else if (args.items[0].kind == VAL_STR) {
		StringBuilder *str = VSTR(args.items[0]);
		for (size_t i = 1; i < args.count; i++) {
			StringBuilder sb = {0};
			val_sprint(args.items[i], &sb, 0);
			sb_appendf(str, "%s", sb.items);
			sb_free(&sb);
		}
	} else err(ctx, cloc, "accepts only list or string");

	return VNONE;
}

Val Remove(EvalCtx *ctx, Location cloc, Vals args) {
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
	err(ctx, cloc, "accepts list and index");
	return VNONE;
}

Val Range(EvalCtx *ctx, Location cloc, Vals args) {
	long long from = 0;
	long long to   = 0;
	long long step = 1;

	if (args.count < 1 || args.count > 3) {
		err(ctx, cloc, "accepts 1, 2 and 3 arguments");
		return VNONE;
	}

	da_foreach (Val, val, &args) {
		if (val->kind != VAL_INT) {
			err(ctx, cloc, "accepts only integers");
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

	Val list = eval_make_val(ctx, VAL_LIST);
	if (step > 0) {
		for (long long i = from; i < to; i += step)
			da_append(VLIST(list), ((Val){ .kind = VAL_INT, .as.vint = i }));
	} else {
		for (long long i = from; i > to; i += step)
			da_append(VLIST(list), ((Val){ .kind = VAL_INT, .as.vint = i }));
	}

	return list;
}

Val Insert(EvalCtx *ctx, Location cloc, Vals args) {
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
	err(ctx, cloc, "accepts: list, index and value");
}

Val Typeof(EvalCtx *ctx, Location cloc, Vals args) {
	if (args.count != 1)
		err(ctx, cloc, "accepts only 1 argument");

	return (Val){
		.kind = VAL_INT,
		.as.vint = args.items[0].kind,
	};
}

Val List(EvalCtx *ctx, Location cloc, Vals args) {
	if (args.count != 1)
		goto error;

	Val list = eval_make_val(ctx, VAL_LIST);
	if (args.items[0].kind == VAL_DICT) {
		ValDict *dict = VDICT(args.items[0]);
		ht_foreach_node (ValDict, dict, n) {
			da_append(VLIST(list), n->key);
		}
	} else if (args.items[0].kind == VAL_STR) {
		char *s = VSTR(args.items[0])->items;
		while (*s) {
			size_t bytes;
			UTF8_Rune cp = utf8_decode(s, &bytes);
			s += bytes;
			da_append(VLIST(list), ((Val){
				.kind = VAL_RUNE,
				.as.vrune = cp,
			}));
		}
	} else goto error;
	return list;

error:
	err(ctx, cloc, "accepts: dictionary or string");
	return VNONE;
}


Val Has(EvalCtx *ctx, Location cloc, Vals args) {
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
	err(ctx, cloc, "accepts: dictionary or list then item");
	return VNONE;
}

void reg_stdlib(EvalCtx *ctx) {
#define REGTYPE(name, knd)               \
	eval_reg_var(ctx, true, name, (Val){ \
		.kind = VAL_INT,                 \
		.as.vint = (knd)                 \
	});
	REGTYPE("_TYPE_NONE_",   VAL_NONE);
	REGTYPE("_TYPE_INT_",    VAL_INT);
	REGTYPE("_TYPE_BOOL_",   VAL_BOOL);
	REGTYPE("_TYPE_FIELD_",  VAL_FIELD);
	REGTYPE("_TYPE_FLOAT_",  VAL_FLOAT);
	REGTYPE("_TYPE_LIST_",   VAL_LIST);
	REGTYPE("_TYPE_DICT_",   VAL_DICT);
	REGTYPE("_TYPE_STR_",    VAL_STR);
	REGTYPE("_TYPE_RUNE_",   VAL_RUNE);
	REGTYPE("_TYPE_CUSTOM_", VAL_CUSTOM);
#undef REGTYPE

	eval_reg_var(ctx, true, "PI", (Val){
		.kind = VAL_FLOAT,
		.as.vfloat = 3.141592653589,
	});

	eval_reg_func(ctx, "json_serialize",   JsonSerialize);
	eval_reg_func(ctx, "json_deserialize", JsonDeserialize);

	eval_reg_func(ctx, "list",    List);
	eval_reg_func(ctx, "len",     Len);
	eval_reg_func(ctx, "int",     Int);
	eval_reg_func(ctx, "pow",     Pow);
	eval_reg_func(ctx, "min",     Min);
	eval_reg_func(ctx, "max",     Max);
	eval_reg_func(ctx, "cos",     Cos);
	eval_reg_func(ctx, "sin",     Sin);
	eval_reg_func(ctx, "floor",   Floor);
	eval_reg_func(ctx, "ceil",    Ceil);
	eval_reg_func(ctx, "sqrt",    Sqrt);
	eval_reg_func(ctx, "split",   Split);
	eval_reg_func(ctx, "bitxor",  BitXor);
	eval_reg_func(ctx, "rune",    Rune);
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
	eval_reg_func(ctx, "typeof",  Typeof);
	eval_reg_func(ctx, "error",   Error);
}
