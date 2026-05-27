#ifndef LEXER_H
#define LEXER_H

#include "error.h"
#include <stdbool.h>
#include "../3dparty/cplus.h"

typedef enum {
	TOK_EOF,
	TOK_ERR,

	TOK_CHAR, TOK_STRING,
	TOK_INT, TOK_FLOAT,
	TOK_TRUE, TOK_FALSE,
	TOK_NONE,

	TOK_SEMI,
	TOK_OPAR, TOK_CPAR,
	TOK_OSQBRA, TOK_CSQBRA,
	TOK_OBRA, TOK_CBRA,

	TOK_ASSIGN, TOK_ID,
	TOK_EXTERN, TOK_IMPORT,

	TOK_ARROW, TOK_ARROW_EQ,

	TOK_FOR_SYM, TOK_IN,
	TOK_BREAK, TOK_CONTINUE,
	TOK_FUNC, TOK_ANY, TOK_RET,
	TOK_IF_SYM, TOK_ELSE_SYM,
	TOK_WHILE_SYM,

	TOK_COM,
	TOK_DOT,
	TOK_COL,

	TOK_EQ,
	TOK_EQ_EQ, TOK_NOT_EQ,
	TOK_LESS, TOK_GREAT,
	TOK_LESS_EQ, TOK_GREAT_EQ,
	TOK_OR, TOK_AND,
	TOK_EXC,

	TOK_PS,
	TOK_PLUS, TOK_MINUS,
	TOK_STAR, TOK_SLASH,

	TOK_PLUS_EQ, TOK_MINUS_EQ,
	TOK_STAR_EQ, TOK_SLASH_EQ,
} TokenKind;

typedef struct {
	TokenKind kind;
	Location loc;
	char *data;
} Token;

typedef struct {
	char *memory;
	char *cur_char;
	Location cur_loc;
	Arena *arena;
} Lexer;

Lexer lexer_from_str(Arena*, char *file, char *code);
Lexer lexer_from_file(Arena*, char *file);
Token lexer_next(Lexer *l);
Token lexer_peek(Lexer *l);
Token lexer_peek2(Lexer *l);
void lexer_free(Lexer *l);

#endif
