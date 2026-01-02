#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include "lexer.h"
#include "../3dparty/cplus.h"

typedef enum {
	AST_OP_EQ,
	AST_OP_ADD,
	AST_OP_SUB,
	AST_OP_MUL,
	AST_OP_DIV,
	AST_OP_NOT_EQ,
	AST_OP_IS_EQ,
	AST_OP_GREAT,
	AST_OP_GREAT_EQ,
	AST_OP_LESS,
	AST_OP_LESS_EQ,
	AST_OP_AND,
	AST_OP_OR,
	AST_OP_ARR,
	AST_OP_PAIR,
} AST_Op;

typedef struct {
	enum {
		LITERAL_INT,
		LITERAL_FLOAT,
		LITERAL_BOOL,
		LITERAL_STR,
	} kind;

	union {
		double vfloat;
		long long vint;
		bool vbool;
		char *vstr;
	} as;
} AST_Literal;

typedef enum {
	FAC_EQ,
	FAC_GREAT,
} FuncArgsKind;

typedef struct {
	enum {
		SYMBOL_VAR,
		SYMBOL_FUNC,
	} kind;
	char *id;

	union {
		struct {
			FuncArgsKind kind;
			size_t count;
		} func;
	} as;
} AST_Symbol;

typedef DA(AST_Symbol) AST_Stack;

typedef struct {
	Lexer lexer;
	AST_Stack stack;
} Parser;

typedef struct AST AST;
typedef DA(AST*) ASTs;
struct AST {
	enum {
		AST_PROG,
		AST_VAR,
		AST_VAR_DEF,
		AST_VAR_MUT,
		AST_LIST,
		AST_DICT,
		AST_FUNC_DEF,
		AST_FUNC_CALL,
		AST_ST_WHILE,
		AST_ST_IF,
		AST_ST_ELSE,
		AST_BIN_EXPR,
		AST_UN_EXPR,
		AST_BODY,
		AST_LIT,
		AST_RET,
		AST_BREAK,
		AST_CONT,
	} kind;

	Location loc;

	union {
		struct {
			AST *cond;
			AST *body;
			AST *chain;
		} st_if_chain;
		struct {
			AST *body;
		} st_else;
		struct {
			AST *cond;
			AST *body;
		} st_while;
		struct {
			char *id;
			AST *expr;
		} var_def;
		struct {
			AST *body;
		} prog;
		struct {
			AST *expr;
		} ret;
		struct {
			AST_Op op;
			AST *lhs;
			AST *rhs;
		} bin_expr;
		struct {
			char *id;
			ASTs args;
		} func_call;
		struct {
			char *id;
			DA(char*) args;
			AST *body;
		} func_def;
		char *var;
		AST *var_mut;
		ASTs body;
		AST_Literal lit;
		ASTs list;
		ASTs dict;
	} as;
};

#define peek(p)  lexer_peek(&p->lexer)
#define next(p)  lexer_next(&p->lexer)
#define peek2(p) lexer_peek2(&p->lexer)

AST *parse(Parser *p);

#endif
