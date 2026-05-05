#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include "lexer.h"
#include "error.h"
#include "../3dparty/cplus.h"
#include "../3dparty/utf8.h"

typedef enum {
	AST_OP_EQ,
	AST_OP_ADD,
	AST_OP_SUB,
	AST_OP_MUL,
	AST_OP_DIV,
	AST_OP_ADD_EQ,
	AST_OP_SUB_EQ,
	AST_OP_MUL_EQ,
	AST_OP_DIV_EQ,
	AST_OP_NOT_EQ,
	AST_OP_IS_EQ,
	AST_OP_GREAT,
	AST_OP_GREAT_EQ,
	AST_OP_LESS,
	AST_OP_LESS_EQ,
	AST_OP_AND,
	AST_OP_OR,
	AST_OP_ARR,
	AST_OP_MOD,
	AST_OP_PAIR,
	AST_OP_NOT,
	AST_OP_NEG,
	AST_OP_FIELD,
	AST_OP_GET_FIELD,
} AST_Op;

typedef struct {
	enum {
		LITERAL_INT,
		LITERAL_FLOAT,
		LITERAL_BOOL,
		LITERAL_STR,
		LITERAL_RUNE,
	} kind;

	union {
		double vfloat;
		long long vint;
		bool vbool;
		char *vstr;
		UTF8_Rune vrune;
	} as;
} AST_Literal;

typedef struct {
	char *id;
	uint uid;
	uint idx;
} AST_Var;

typedef struct {
	enum {
		AST_SBL_VAR,
		AST_SBL_FUNC,
	} kind;

	AST_Var var;
} AST_Symbol;

typedef struct AST AST;
typedef DA(AST*) ASTs;

typedef struct {
	Lexer lexer;
	ErrorCtx err_ctx;
	DA(AST_Symbol) symbol_table;
	size_t stack_ptr;
} Parser;

struct AST {
	enum {
		AST_PROG,
		AST_VAR,
		AST_VAR_ANY,
		AST_VAR_DEF,
		AST_VAR_MUT,
		AST_VAL_NONE,
		AST_LIST,
		AST_DICT,
		AST_FUNC_DEF,
		AST_FUNC_CALL,
		AST_ST_WHILE,
		AST_ST_FOR,
		AST_ST_FOREACH,
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
			AST *var;
			AST *cond;
			AST *mut;
			AST *body;
		} st_for;
		struct {
			AST_Var var;
			AST *coll;
			AST *body;
		} st_foreach;
		struct {
			AST_Var var;
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
			AST_Op op;
			AST *v;
		} un_expr;
		struct {
			AST_Var var;
			ASTs args;
		} func_call;
		struct {
			AST_Var var;
			ASTs args;
			AST *body;
		} func_def;
		struct {
			ASTs stmts;
			bool scope;
		} body;
		AST_Var var;
		AST *var_mut;
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
