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
} AST_Op;

typedef struct {
	enum {
		LITERAL_INT,
		LITERAL_FLOAT,
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
		struct {
			Type type;
		} var;
	} as;
} Symbol;

typedef DA(Symbol) SymbolStack;

typedef struct {
	Lexer *lexer;
	SymbolStack ss;
} Parser;

typedef struct AST AST;
typedef DA(AST*) ASTs;
struct AST {
	enum {
		AST_PROG,
		AST_VAR,
		AST_VAR_DEF,
		AST_VAR_MUT,
		AST_ST_WHILE,
		AST_ST_IF,
		AST_BIN_EXPR,
		AST_UN_EXPR,
		AST_BODY,
		AST_LIT,
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
			AST_Op op;
			AST *lhs;
			AST *rhs;
		} bin_expr;
		char *var;
		AST *var_mut;
		ASTs body;
		Literal lit;
	} as;
};

AST *parse_expr(Parser *p);
AST *parser_parse(Lexer *lexer);

#endif
