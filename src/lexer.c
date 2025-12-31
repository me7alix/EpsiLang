#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>
#include "../include/lexer.h"
#include "../3dparty/cplus.h"

char *get_word(Lexer *lexer) {
	while (*lexer->cur_char == ' ')
		lexer->cur_char++;

	char *start = lexer->cur_char;
	while (isalpha(*lexer->cur_char) ||
		isdigit(*lexer->cur_char) ||
		*lexer->cur_char == '_')
		lexer->cur_char++;
	lexer->cur_char--;

	size_t l = lexer->cur_char - start + 1;
	char *word = malloc(sizeof(char) * (l+1));
	memcpy(word, start, l);
	word[l] = '\0';
	return word;
}

Token token(Lexer *lexer, TokenKind kind, char *data) {
	return (Token) {
		.kind = kind,
		.data = data,
		.loc = lexer->cur_loc,
	};
}

bool is_tok(Lexer *lexer, const char *tok) {
	char *str = lexer->cur_char;

	for (size_t i = 0; i < strlen(tok); i++) {
		if (tok[i] != str[i]) return false;
	}

	if (isalpha(str[strlen(tok)]) || str[strlen(tok)] == '_')
		return false;

	return true;
}

void lexer_error(Location loc, char *error) {
	size_t lines_num = loc.line_num + 1;
	size_t chars_num = loc.line_char-loc.line_start + 1;
	printf("%s:%zu:%zu: %s\n", loc.file, lines_num, chars_num, error);

	loc.line_char = loc.line_start;
	char error_pointer[128];
	size_t cnt = 0;

	while (*loc.line_char != '\n' && *loc.line_char != '\0'){
		printf("%c", *loc.line_char);
		if (cnt < chars_num - 1) {
			if (*loc.line_char != '\t')
				error_pointer[cnt++] = ' ';
			else
				error_pointer[cnt++] = '\t';
		}
		loc.line_char++;
	}

	printf("\n");
	error_pointer[cnt++] = '^';
	error_pointer[cnt] = '\0';
	printf("%s\n", error_pointer);
	exit(1);
}

Lexer lexer_init(char *file, char *code) {
	return (Lexer) {
		.cur_loc.file = file,
		.cur_loc.line_num = 0,
		.cur_loc.line_start = code,
		.cur_loc.line_char = code,
		.cur_char = code,
	};
}

struct {
	const char *id;
	TokenKind kind;
} keywordPairs[] = {
	{ "for",      TOK_FOR_SYM   },
	{ "while",    TOK_WHILE_SYM },
	{ "if",       TOK_IF_SYM    },
	{ "else",     TOK_ELSE_SYM  },
	{ "extern",   TOK_EXTERN    },
	{ "true",     TOK_TRUE      },
	{ "false",    TOK_FALSE     },
	{ "break",    TOK_BREAK     },
	{ "continue", TOK_CONTINUE  },
	{ "return",   TOK_RET       },
	{ "import",   TOK_IMPORT    },
	{ "fn",       TOK_FUNC      },
};

Token lexer_next(Lexer *l) {
	Token ret;
	l->cur_loc.line_char = l->cur_char;
	ret.loc = l->cur_loc;

	switch (*l->cur_char) {
		case ' ': case '\t':
			l->cur_char++;
			return lexer_next(l);

		case '\0':
			ret = token(l, TOK_EOF, "EOF");
			break;

		case '{': ret = token(l, TOK_OBRA,  "{"); break;
		case '}': ret = token(l, TOK_CBRA,  "}"); break;
		case '(': ret = token(l, TOK_OPAR,  "("); break;
		case ')': ret = token(l, TOK_CPAR,  ")"); break;
		case ';': ret = token(l, TOK_SEMI,  ";"); break;
		case ',': ret = token(l, TOK_COM,   ","); break;
		case '[': ret = token(l, TOK_OSQBRA,"["); break;
		case ']': ret = token(l, TOK_CSQBRA,"]"); break;
		case '%': ret = token(l, TOK_PS,    "%"); break;
		case '#': ret = token(l, TOK_MACRO, "#"); break;
		case '^': ret = token(l, TOK_XOR,   "^"); break;
		case '~': ret = token(l, TOK_TILDA, "~"); break;

		case '.': {
			if (l->cur_char[1] == '.' && l->cur_char[2] == '.') {
				ret = token(l, TOK_ANY, "...");
				l->cur_char += 2;
			} else ret = token(l, TOK_DOT, ".");
		} break;

		case '+': {
			if (l->cur_char[1] == '=') {
				ret = token(l, TOK_PLUS_EQ, "+=");
				l->cur_char++;
			} else ret = token(l, TOK_PLUS, "+");
		} break;

		case '-': {
			if (l->cur_char[1] == '=') {
				ret = token(l, TOK_MINUS_EQ, "-=");
				l->cur_char++;
			} else ret = token(l, TOK_MINUS, "-");
		} break;

		case '*': {
			if (l->cur_char[1] == '=') {
				ret = token(l, TOK_STAR_EQ, "*=");
				l->cur_char++;
			} else ret = token(l, TOK_STAR, "*");
		} break;

		case '/': {
			if (l->cur_char[1] == '/') {
				while (l->cur_char[1] != '\n')
					l->cur_char++;
			} else if (l->cur_char[1] == '=') {
				ret = token(l, TOK_SLASH_EQ, "/=");
				l->cur_char++;
			} else ret = token(l, TOK_SLASH, "/");
		} break;

		case '!': {
			if (l->cur_char[1] == '=') {
				ret = token(l, TOK_NOT_EQ, "!=");
				l->cur_char++;
			} else {
				ret = token(l, TOK_EXC, "!");
			}
		} break;

		case '&': {
			if (l->cur_char[1] == '&') {
				ret = token(l, TOK_AND, "&&");
				l->cur_char++;
			} else {
				ret = token(l, TOK_AMP, "&");
			}
		} break;

		case '|': {
			if (l->cur_char[1] == '|') {
				ret = token(l, TOK_OR, "||");
				l->cur_char++;
			} else {
				ret = token(l, TOK_PIPE, "|");
			}
		} break;

		case '>': {
			if (l->cur_char[1] == '=') {
				ret = token(l, TOK_GREAT_EQ, ">=");
				l->cur_char++;
			} else if (l->cur_char[1] == '>') {
				ret = token(l, TOK_RIGHT_SHIFT, ">>");
				l->cur_char++;
			} else {
				ret = token(l, TOK_GREAT, ">");
			}
		} break;

		case '<': {
			if (l->cur_char[1] == '=') {
				ret = token(l, TOK_LESS_EQ, "<=");
				l->cur_char++;
			} else if (l->cur_char[1] == '<') {
				ret = token(l, TOK_LEFT_SHIFT, "<<");
				l->cur_char++;
			} else {
				ret = token(l, TOK_LESS, "<");
			}
		} break;

		case '=': {
			if (l->cur_char[1] == '=') {
				ret = token(l, TOK_EQ_EQ, "==");
				l->cur_char++;
			} else {
				ret = token(l, TOK_EQ, "=");
			}
		} break;

		case '\r':
		case '\n': {
			if (l->cur_char[0] == '\r' && l->cur_char[1] == '\n')
				l->cur_char++;

			l->cur_loc.line_num++;
			l->cur_loc.line_start = l->cur_char + 1;

			l->cur_char++;
			return lexer_next(l);
		} break;

		case ':': {
			if (l->cur_char[1] == '=') {
				ret = token(l, TOK_ASSIGN, ":=");
				l->cur_char++;
			} else {
				ret = token(l, TOK_COL, ":");
			}
		} break;

		default: {
			if (isdigit(*l->cur_char)) {
				char *start = l->cur_char;
				bool isFloat = 0;
				while (true) {
					if (*l->cur_char == '.')
						isFloat = 1;
					if (!(isdigit(l->cur_char[1]) ||
						isalpha(l->cur_char[1]) ||
						l->cur_char[1] == '.')) break;
					l->cur_char++;
				}

				size_t len = l->cur_char - start + 1;
				char *num = malloc(sizeof(char) * (len+1));
				memcpy(num, start, len); num[len] = '\0';
				if (isFloat) ret = token(l, TOK_FLOAT, num);
				else         ret = token(l, TOK_INT, num);
			}

			else if (*(l->cur_char) == '"') {
				StringBuilder sb = {0};
				l->cur_char++;

				while (!(l->cur_char[0] == '\"' && l->cur_char[-1] != '\\')) {
					if (l->cur_char[0] == '\\') {
						switch (l->cur_char[1]) {
							case '\\': sb_append(&sb, '\\'); break;
							case '0':  sb_append(&sb, '\0'); break;
							case 'n':  sb_append(&sb, '\n'); break;
							case '\"': sb_append(&sb, '\"'); break;
							default: lexer_error(l->cur_loc, "error: wrong character");
						}
						l->cur_char++;
					} else if (l->cur_char[0] == '\0') {
						lexer_error(l->cur_loc, "error: unclosed string");
					} else {
						sb_append(&sb, l->cur_char[0]);
					}

					l->cur_char++;
				}

				sb_append(&sb, '\0');
				ret = token(l, TOK_STRING, sb.items);
			}

			else if (*l->cur_char == '\'') {
				l->cur_char++;
				if (*l->cur_char == '\\') {
					l->cur_char++;
					switch (*l->cur_char) {
						case '0':  ret = token(l, TOK_CHAR, "\0"); break;
						case 'n':  ret = token(l, TOK_CHAR, "\n"); break;
						case '\\': ret = token(l, TOK_CHAR, "\\"); break;
						case '\'': ret = token(l, TOK_CHAR, "'");  break;
						default: lexer_error(l->cur_loc, "error: wrong character");
					}
				} else ret = token(l, TOK_CHAR, l->cur_char);

				l->cur_char++;
				if (*l->cur_char != '\'')
					lexer_error(l->cur_loc, "error: ' expected");
			}

			else if (isalpha(*l->cur_char)) {
				for (size_t i = 0; i < ARR_LEN(keywordPairs); i++) {
					const char *kp = keywordPairs[i].id;
					if (is_tok(l, kp)) {
						ret = token(l, keywordPairs[i].kind, (char*)kp);
						for (size_t i = 0; i < strlen(kp)-1; i++) l->cur_char++;
						goto exit;
					}
				}

				ret = token(l, TOK_ID, get_word(l));
			}

			else lexer_error(l->cur_loc, "error: unknown token");
		} break;
	}

exit:
	l->cur_char++;
	return ret;
}

Token lexer_peek(Lexer *l) {
	Lexer pl = *l;
	Token t = lexer_next(l);
	*l = pl;
	return t;
}

Token lexer_peek2(Lexer *l) {
	Lexer pl = *l;
	lexer_next(l);
	Token t = lexer_next(l);
	*l = pl;
	return t;
}
