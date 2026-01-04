#ifndef EPSL_API_H
#define EPSL_API_H

#include <stdbool.h>
#include <stdlib.h>

typedef struct {
	char *file;
	size_t line_num;
	char *line_start;
	char *line_char;
} EpslLocation;

typedef enum {
	EPSL_ERROR_COMPTIME,
	EPSL_ERROR_RUNTIME,
} EpslErrorKind;

typedef void (*EpslErrorFn)(EpslLocation loc, EpslErrorKind ek, char *msg);

typedef struct {
	EpslErrorFn errf;
	bool got_err;
} EpslErrorCtx;

typedef enum {
	EPSL_FAC_EQ,
	EPSL_FAC_GREAT,
} EpslFuncArgsKind;

typedef void EpslCtx;
typedef void EpslValDict;
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
		EpslValDict *dict;
		EpslString *str;
	} as;
};

typedef struct {
	EpslVal val;
	bool got_err;
} EpslResult;

EpslCtx *epsl_from_str(EpslErrorFn errf, char *code);
EpslCtx *epsl_from_file(EpslErrorFn errf, char *filename);
EpslResult epsl_eval(EpslCtx *ctx);
void epsl_print_ast(EpslCtx *ctx);
void epsl_print_tokens(EpslCtx *ctx);

typedef EpslVal (*EpslRegFunc)(EpslLocation call_loc, EpslErrorCtx *ec, EpslVals args);
void epsl_reg_var(EpslCtx *ctx, const char *id, EpslVal val);
void epsl_reg_func(EpslCtx *ctx, const char *name, EpslRegFunc rf);

#endif
