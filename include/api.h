#ifndef EPSL_API_H
#define EPSL_API_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
	char *file;
	size_t line_num;
	char *line_start;
	char *line_char;
} EpslLocation;

typedef enum : uint8_t {
	EPSL_ERROR_COMPTIME,
	EPSL_ERROR_RUNTIME,
} EpslErrorKind;

typedef void (*EpslErrorFn)(EpslLocation loc, EpslErrorKind ek, char *msg);

typedef struct {
	EpslErrorFn errf;
	bool got_err;
} EpslErrorCtx;

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
		EPSL_VAL_NONE,
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
		void *gc_obj;
	} as;
};

typedef struct {
	EpslVal val;
	bool got_err;
} EpslResult;

typedef struct EpslEvalCtx EpslEvalCtx;
typedef EpslVal (*EpslRegFunc)(EpslEvalCtx *ctx, EpslLocation call_loc, EpslVals args);

EpslVal epsl_new_heap_val(EpslCtx *ctx, uint8_t kind);
EpslCtx *epsl_from_str(EpslErrorFn errf, char *code);
EpslCtx *epsl_from_file(EpslErrorFn errf, char *filename);
EpslResult epsl_eval(EpslCtx *ctx);

void epsl_print_ast(EpslCtx *ctx);
void epsl_print_tokens(EpslCtx *ctx);

void epsl_reg_var(EpslCtx *ctx, const char *id, EpslVal val);
void epsl_reg_func(EpslCtx *ctx, const char *name, EpslRegFunc rf);

char *epsl_val_get_str(EpslVal *v);
void epsl_val_set_str(EpslCtx *ctx, EpslVal *v, char *str);

#endif
