#ifndef EXEC_H
#define EXEC_H

#include <stdbool.h>
#include "../include/parser.h"

typedef struct Val Val;
typedef DA(Val) Vals;
typedef Val (*RegFunc)(Vals args);

struct Val {
	enum {
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
		Vals *list;
		void *dict;
		StringBuilder *str;
	} as;
};

HT_DECL(ValDict, Val, Val);

typedef struct {
	enum {
		EVAL_SYMB_VAR,
		EVAL_SYMB_FUNC,
		EVAL_SYMB_REG_FUNC,
	} kind;
	char *id;

	union {
		struct {
			Val val;
		} var;
		struct {
			AST *node;
		} func;
		RegFunc reg_func;
	} as;
} EvalSymbol;

typedef DA(EvalSymbol) EvalStack;

typedef struct {
	enum {
		EXEC_CTX_NONE,
		EXEC_CTX_RET,
		EXEC_CTX_BREAK,
		EXEC_CTX_CONT,
	} state;

	EvalStack stack;
} EvalCtx;

Val eval(EvalCtx *ex, AST *n);
void reg_func(
	Parser *p, EvalCtx *ex,
	RegFunc rf, char *name,
	FuncArgsKind fk, size_t cnt);

#endif
