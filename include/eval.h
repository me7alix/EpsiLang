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

typedef struct Val Val;
typedef DA(Val) Vals;

struct Val {
	enum {
		VAL_NONE,
		VAL_INT,
		VAL_FLOAT,
		VAL_BOOL,
		VAL_STR,
		VAL_LIST,
		VAL_DICT,
	} kind;
	
	union {
		long long vint;
		double vfloat;
		bool vbool;
		GC_Object *gc_obj;
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
		EVAL_SYMB_TEMP,
		EVAL_SYMB_VAR,
		EVAL_SYMB_FUNC,
		EVAL_SYMB_REG_FUNC,
	} kind;
	char *id;

	union {
		struct { Val val;   } var;
		struct { Val val;   } temp;
		struct { AST *node; } func;
		RegFunc reg_func;
	} as;
} EvalSymbol;

typedef DA(EvalSymbol) EvalStack;

struct EvalCtx {
	enum {
		EVAL_CTX_NONE,
		EVAL_CTX_RET,
		EVAL_CTX_BREAK,
		EVAL_CTX_CONT,
	} state;

	GarbageCollector gc;
	EvalStack stack;
	ErrorCtx err_ctx;
};

void eval_collect_garbage(EvalCtx *ctx);
Val eval_new_heap_val(EvalCtx *ctx, int kind);

Val eval(EvalCtx *ctx, AST *n);
void eval_reg_var(EvalCtx *ctx, const char *id, Val val);
void eval_reg_func(EvalCtx *ctx, const char *id, RegFunc rf);

#endif
