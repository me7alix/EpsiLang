#ifndef EPSL_API_H
#define EPSL_API_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/* Source code location used for error messages */
typedef struct {
	char *file;
	size_t line_num;
	char *line_start;
	char *line_char;
} EpslLocation;

/* Call stack for error messages */
typedef struct {
	char *name;
	EpslLocation loc;
} EpslCall;

typedef struct {
	EpslCall *items;
	size_t count;
	size_t capacity;
	void *arena;
} EpslCallStack;

/* Type of error reported by the interpreter */
typedef enum {
	EPSL_ERROR_COMPTIME,
	EPSL_ERROR_RUNTIME,
} EpslErrorKind;

/* Error callback used by the API to report diagnostics */
typedef void (*EpslErrorFn)(
	EpslCallStack *cs,
	EpslLocation loc,
	EpslErrorKind ek,
	char *msg);

typedef struct {
	EpslErrorFn errf;
	bool got_err;
} EpslErrorCtx;

typedef void EpslCtx;
typedef struct EpslVal EpslVal;
typedef struct EpslEvalCtx EpslEvalCtx;

typedef struct {
	EpslVal *items;
	size_t count;
	size_t capacity;
	void *arena;
} EpslVals;

typedef struct {
	char *items;
	size_t count;
	size_t capacity;
	void *arena;
} EpslString;

/* Runtime value representation */
struct EpslVal  {
	enum {
		EPSL_VAL_NONE,
		EPSL_VAL_INT,
		EPSL_VAL_FLOAT,
		EPSL_VAL_BOOL,
		EPSL_VAL_FIELD,
		EPSL_VAL_RUNE,
		EPSL_VAL_STR,
		EPSL_VAL_LIST,
		EPSL_VAL_DICT,
		EPSL_VAL_CUSTOM,
	} kind;

	union {
		long long vint;
		double vfloat;
		bool vbool;
		uint32_t vrune;
		char *field;
		void *gc_obj;
	} as;
};

/* Custom object */
typedef struct {
	int kind;
	void *data;
	void (*free)(void *ptr);
} EpslCustomObj;

/* Result of evaluation */
typedef struct {
	EpslVal val;
	bool got_err;
} EpslResult;

/* Function pointer type for evaluator-registered functions */
typedef EpslVal (*EpslRegFunc)(
	EpslEvalCtx *ctx,
	EpslLocation call_loc,
	EpslVals args);

#define EPSL_NONE ((EpslVal){0})

/* Create a value (string, array, dictionary) from an evaluator context and kind */
EpslVal epsl_eval_make_value(EpslEvalCtx *ctx, int kind);

/* Create a value from an interpreter context and kind */
EpslVal epsl_make_value(EpslCtx *ctx, int kind);

/* Create a custom object from an evaluator context */
EpslVal epsl_eval_make_custom(EpslEvalCtx *ctx, EpslCustomObj custom);

/* Create a custom object from an interpreter context */
EpslVal epsl_make_custom(EpslCtx *ctx, EpslCustomObj custom);

/* Create an interpreter context from source code text */
EpslCtx *epsl_from_str(EpslErrorFn errf, char *code);

/* Create an interpreter context from a file */
EpslCtx *epsl_from_file(EpslErrorFn errf, char *filename);

/* Evaluate the program stored in the interpreter context */
EpslResult epsl_eval(EpslCtx *ctx);

/* Print the abstract syntax tree for debugging */
void epsl_print_ast(EpslCtx *ctx);

/* Print the token stream for debugging */
void epsl_print_tokens(EpslCtx *ctx);

/* Register a global variable in the interpreter */
void epsl_reg_var(EpslCtx *ctx, bool is_const, const char *id, EpslVal val);

/* Register a native/runtime function in the interpreter */
void epsl_reg_func(EpslCtx *ctx, const char *name, EpslRegFunc rf);

/* Get a string object from a value */
EpslString *epsl_val_get_str(EpslVal val);

/* Set the string contents of a value */
void epsl_val_set_str(EpslVal val, char *str);

/* Append a value to a list value */
void epsl_val_list_append(EpslVal list, EpslVal v);

/* Get a custom object from a value */
EpslCustomObj epsl_val_get_custom(EpslVal v);

/* Throw a runtime error */
void epsl_throw_error(EpslEvalCtx *ctx, EpslLocation loc, char *msg);

/* Free all used memory */
void epsl_free(EpslCtx *ctx);

#endif
