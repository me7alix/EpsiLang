#ifndef EPSL_API_H
#define EPSL_API_H

#include <stdbool.h>
#include <stdlib.h>

typedef enum {
	EPSL_FAC_EQ,
	EPSL_FAC_GREAT,
} EpslFuncArgsKind;

typedef void EpslCtx;
typedef struct EpslVal EpslVal;

typedef struct {
	EpslVal *items;
	size_t count;
	size_t capacity;
} EpslVals;

typedef struct {
	char *items;
	size_t count;
	size_t capacity;
} EpslString;

struct EpslVal {
	enum {
		EPSL_VAL_INT,
		EPSL_VAL_FLOAT,
		EPSL_VAL_BOOL,
		EPSL_VAL_STR,
		EPSL_VAL_LIST,
		EPSL_VAL_DICT,
	} kind;
	
	union {
		long long vint;
		double vfloat;
		bool vbool;
		EpslVals *list;
		void *dict;
		EpslString *str;
	} as;
};


EpslCtx *epsl_from_str(char *code);
EpslCtx *epsl_from_file(char *filename);
EpslVal epsl_eval(EpslCtx *ctx);
void epsl_print_ast(EpslCtx *ctx);
void epsl_print_tokens(EpslCtx *ctx);

typedef EpslVal (*EpslRegFunc)(EpslVals args);
void epsl_reg_func(
	EpslCtx *ctx,
	EpslRegFunc rf, char *name,
	EpslFuncArgsKind fk, size_t cnt);

#endif
