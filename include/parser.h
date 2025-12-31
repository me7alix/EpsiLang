#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "type.h"
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
	AST_OP_LESS_EQ,
} AST_Op;

typedef struct {
	enum {
		LITERAL_INT,
		LITERAL_FLOAT,
		LITERAL_BOOL,
	} kind;

	union {
		double float_val;
		long long int_val;
	} as;
} Literal;

typedef struct {
	enum {
		SYMBOL_VAR,
		SYMBOL_FUNC,
	} kind;
	char *id;

	union {
		Type var_type;
		size_t func_args_cnt;
	} as;
} Symbol;

typedef DA(Symbol) SymbolStack;

typedef struct {
	Lexer lexer;
	SymbolStack stack;
} Parser;

typedef struct AST AST;
typedef DA(AST*) ASTs;
struct AST {
	enum {
		AST_PROG,
		AST_VAR,
		AST_VAR_DEF,
		AST_VAR_MUT,
		AST_FUNC_DEF,
		AST_FUNC_CALL,
		AST_ST_WHILE,
		AST_ST_IF,
		AST_BIN_EXPR,
		AST_UN_EXPR,
		AST_BODY,
		AST_LIT,
		AST_RET,
	} kind;

	Location loc;
	Type type;

	union {
		struct {
			AST *cond;
			AST *body;
		} st_if;
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
		Literal lit;
	} as;
};

#define peek(p)  lexer_peek(&p->lexer)
#define next(p)  lexer_next(&p->lexer)
#define peek2(p) lexer_peek2(&p->lexer)

AST *parse(Parser *p);

#endif
