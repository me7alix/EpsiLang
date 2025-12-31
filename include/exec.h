#ifndef EXEC_H
#define EXEC_H

#include "../include/parser.h"


typedef struct {
	Type type;
	
	union {
		long long vint;
		double vfloat;
		bool vbool;
		char *data;
	} as;
} Val;

typedef DA(Val) Vals;
typedef Val (*RegFunc)(Vals args);

typedef struct {
	enum {
		EXEC_SYMB_VAR,
		EXEC_SYMB_FUNC,
		EXEC_SYMB_REG_FUNC,
	} kind;
	char *id;

	union {
		struct {
			Val val;
		} var;
		struct {
			AST *node;
			size_t args_cnt;
		} func;
		RegFunc reg_func;
	} as;
} ExecSymbol;

typedef DA(ExecSymbol) ExecStack;

typedef struct {
	ExecStack stack;
} Exec;

typedef struct {
	enum {
		EXEC_CTX_NONE,
		EXEC_CTX_RET,
		EXEC_CTX_BREAK,
		EXEC_CTX_CONT,
	} kind;

	Val val;
} ExecCtx;

void exec_reg_func(Exec *ex, RegFunc rf, char *name);
ExecCtx exec(Exec *ex, AST *n);

#endif
