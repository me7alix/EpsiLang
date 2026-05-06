#ifndef EXEC_H
#define EXEC_H

#include <stdbool.h>
#include <stddef.h>
#include "error.h"
#include "../include/parser.h"

typedef struct {
	bool marked;
	int val_kind;
	void *data;
} GC_Object;

#define GC_INIT_THRESHOLD 256
#define GC_GROWTH_FACTOR 2
#define GC_MIN_GROWTH 64

typedef struct {
	DA(GC_Object*) objs;
	size_t threshold;
	Arena from, to;
} GarbageCollector;

typedef struct {
	int kind;
	void *data;
	void (*free)(void *ptr);
} EvalCustomObj;

typedef struct Val Val;
typedef DA(Val) Vals;

struct Val {
	enum {
		VAL_NONE,
		VAL_INT,
		VAL_FLOAT,
		VAL_BOOL,
		VAL_FIELD,
		VAL_RUNE,
		VAL_STR,
		VAL_LIST,
		VAL_DICT,
		VAL_CUSTOM,
		_VAL_FUNC,
	} kind;
	
	union {
		long long vint;
		double vfloat;
		bool vbool;
		UTF8_Rune vrune;
		char *field;
		GC_Object *gc_obj;
		AST *func;
	} as;
};

typedef struct EvalCtx EvalCtx;
typedef Val (*RegFunc)(EvalCtx *ctx, Location call_loc, Vals args);

#define VNONE ((Val){0})
#define VDICT(v) ((ValDict*)v.as.gc_obj->data)
#define VLIST(v) ((Vals*)v.as.gc_obj->data)
#define VSTR(v) ((StringBuilder*)v.as.gc_obj->data)

HT_DECL(ValDict, Val, Val);

typedef struct {
	enum {
		REG_VAR,
		REG_FUNC,
	} kind;

	union {
		Val var;
		RegFunc func;
	} as;
} RegSymbol;

typedef struct {
	AST_Var var;
	Val val;
} EvalVar;

typedef DA(EvalVar) EvalScope;

HT_DECL_STR(RegSymbols, RegSymbol)

struct EvalCtx {
	enum {
		EVAL_CTX_NONE,
		EVAL_CTX_RET,
		EVAL_CTX_BREAK,
		EVAL_CTX_CONT,
	} state;

	GarbageCollector gc;
	RegSymbols reg_sbls;
	DA(EvalScope) var_stack;
	DA(Val) temp_stack;
	ErrorCtx err_ctx;
};

void eval_collect_garbage(EvalCtx *ctx);
Val eval_make_val(EvalCtx *ctx, int kind);
Val eval_gc_new_custom(EvalCtx *ctx, EvalCustomObj custom);

Val eval(EvalCtx *ctx, AST *n);
void eval_reg_var(EvalCtx *ctx, const char *id, Val val);
void eval_reg_func(EvalCtx *ctx, const char *id, RegFunc rf);

#endif
