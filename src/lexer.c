#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>
#include "../include/lexer.h"
#include "../3dparty/cplus.h"
#include "../3dparty/utf8.h"

char *read_file(const char *filename);
static char  chp(Lexer *l) { return *l->cur_char;   }
static char  chn(Lexer *l) { return *l->cur_char++; }
static char *chl(Lexer *l) { return  l->cur_char;   }

char *dup(Lexer *l, char *str) {
	return arena_strdup(l->arena, str);
}

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
	char *word = arena_alloc(lexer->arena, sizeof(char) * (l+1));
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

	if (isalpha(str[strlen(tok)]) || str[strlen(tok)] == '_') {
		return false;
	}

	return true;
}

Lexer lexer_from_str(Arena *arena, char *file, char *code) {
	size_t len = strlen(code) + 1;
	char *memory = malloc(len);
	memcpy(memory, code, len);

	return (Lexer) {
		.arena = arena,
		.memory = memory,
		.cur_loc.file = file,
		.cur_loc.line_num = 0,
		.cur_loc.line_start = memory,
		.cur_loc.line_char = memory,
		.cur_char = memory,
	};
}

Lexer lexer_from_file(Arena *arena, char *file) {
	char *code = read_file(file);
	return (Lexer) {
		.arena = arena,
		.memory = code,
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
	{ "in",       TOK_IN        },
	{ "or",       TOK_OR        },
	{ "do",       TOK_ARROW     },
	{ "and",      TOK_AND       },
	{ "none",     TOK_NONE      },
};

Token lexer_next(Lexer *l) {
	Token ret;
	l->cur_loc.line_char = l->cur_char;
	ret.loc = l->cur_loc;

	switch (chp(l)) {
		case ' ':
		case '\t':
			chn(l);
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

		case '.': {
			if (chl(l)[1] == '.' && chl(l)[2] == '.') {
				ret = token(l, TOK_ANY, "...");
				chn(l); chn(l);
			} else ret = token(l, TOK_DOT, ".");
		} break;

		case '+': {
			if (chl(l)[1] == '=') {
				ret = token(l, TOK_PLUS_EQ, "+=");
				chn(l);
			} else ret = token(l, TOK_PLUS, "+");
		} break;

		case '-': {
			if (chl(l)[1] == '=') {
				ret = token(l, TOK_MINUS_EQ, "-=");
				chn(l);
			} else ret = token(l, TOK_MINUS, "-");
		} break;

		case '*': {
			if (chl(l)[1] == '=') {
				ret = token(l, TOK_STAR_EQ, "*=");
				chn(l);
			} else ret = token(l, TOK_STAR, "*");
		} break;

		case '/': {
			if (chl(l)[1] == '/') {
				while (chp(l) != '\n')
					chn(l);
				return lexer_next(l);
			} else if (chl(l)[1] == '=') {
				ret = token(l, TOK_SLASH_EQ, "/=");
				chn(l);
			} else ret = token(l, TOK_SLASH, "/");
		} break;

		case '!': {
			if (chl(l)[1] == '=') {
				ret = token(l, TOK_NOT_EQ, "!=");
				chn(l);
			} else {
				ret = token(l, TOK_EXC, "!");
			}
		} break;

		case '>': {
			if (chl(l)[1] == '=') {
				ret = token(l, TOK_GREAT_EQ, ">=");
				chn(l);
			} else {
				ret = token(l, TOK_GREAT, ">");
			}
		} break;

		case '<': {
			if (chl(l)[1] == '=') {
				ret = token(l, TOK_LESS_EQ, "<=");
				chn(l);
			} else {
				ret = token(l, TOK_LESS, "<");
			}
		} break;

		case '=': {
			if (chl(l)[1] == '=') {
				ret = token(l, TOK_EQ_EQ, "==");
				chn(l);
			} else if (chl(l)[1] == '>') {
				ret = token(l, TOK_ARROW_EQ, "=>");
				chn(l);
			} else {
				ret = token(l, TOK_EQ, "=");
			}
		} break;

		case '\r':
		case '\n': {
			if (chl(l)[0] == '\r' && chl(l)[1] == '\n')
				chn(l);
			chn(l);
			l->cur_loc.line_num++;
			l->cur_loc.line_start = chl(l);
			return lexer_next(l);
		} break;

		case ':': {
			if (chl(l)[1] == '=') {
				ret = token(l, TOK_ASSIGN, ":=");
				chn(l);
			} else {
				ret = token(l, TOK_COL, ":");
			}
		} break;

		case '\'': {
			chn(l);
			if (chl(l)[0] == '\\') {
				char ch;
				chn(l);
				switch (chl(l)[0]) {
				case '\\': ch = '\\'; break;
				case '0':  ch = '\0'; break;
				case 'n':  ch = '\n'; break;
				case 'r':  ch = '\r'; break;
				case 't':  ch = '\t'; break;
				case '\'': ch = '\''; break;
				default:
					ret = token(l, TOK_ERR, "invalid rune");
					goto exit;
				}

				char str[5]; sprintf(str, "%c", ch);
				ret = token(l, TOK_CHAR, dup(l, str));
				chn(l);
			} else {
				size_t bytes_read;
				utf8_decode(chl(l), &bytes_read);

				char *chr = arena_alloc(l->arena, bytes_read + 1);
				memcpy(chr, chl(l), bytes_read);
				chr[bytes_read] = '\0';

				ret = token(l, TOK_CHAR, chr);
				for (size_t i = 0; i < bytes_read; i++) chn(l);
			}

			if (chl(l)[0] != '\'') {
				ret = token(l, TOK_ERR, "rune should be closed with '");
			}
		} break;

		default: {
			if (isdigit(chp(l))) {
				char *start = chl(l);
				bool isFloat = 0;
				while (true) {
					if (chp(l) == '.')
						isFloat = 1;
					if (!(isdigit(chl(l)[1]) ||
						isalpha(chl(l)[1]) ||
						chl(l)[1] == '.')) break;
					chn(l);
				}

				size_t len = chl(l) - start + 1;
				char *num = arena_alloc(l->arena, sizeof(char) * (len+1));
				memcpy(num, start, len); num[len] = '\0';
				if (isFloat) ret = token(l, TOK_FLOAT, num);
				else         ret = token(l, TOK_INT, num);
			}

			else if (chp(l) == '"') {
				StringBuilder sb = {.arena = l->arena};
				chn(l);

				while (chl(l)[0] != '\"') {
					if (chl(l)[0] == '\\') {
						chn(l);
						switch (chp(l)) {
						case '\\': sb_append(&sb, '\\'); break;
						case '0':  sb_append(&sb, '\0'); break;
						case 'n':  sb_append(&sb, '\n'); break;
						case 'r':  sb_append(&sb, '\r'); break;
						case 't':  sb_append(&sb, '\t'); break;
						case '\"': sb_append(&sb, '\"'); break;
						default:
							ret = token(l, TOK_ERR, "string contains invalid character");
							goto exit;
						}
					} else if (chp(l) == '\0') {
						ret = token(l, TOK_ERR, "unclosed string");
						break;
					} else {
						sb_append(&sb, chp(l));
					}

					chn(l);
				}

				sb_append(&sb, '\0');
				ret = token(l, TOK_STRING, sb.items);
			}

			else if (chp(l) == '\'') {
				chn(l);
				if (chn(l) == '\\') {
					char *chr;
					switch (chp(l)) {
					case '0':  chr = "\0"; break;
					case 'n':  chr = "\n"; break;
					case 'r':  chr = "\r"; break;
					case 't':  chr = "\t"; break;
					case '\\': chr = "\\"; break;
					case '\'': chr = "'";  break;
					default:
						ret = token(l, TOK_ERR, "invalid character");
						goto exit;
					}
					ret = token(l, TOK_CHAR, dup(l, chr));
				} else {
					char buf[2];
					sprintf(buf, "%c", chp(l));
					ret = token(l, TOK_CHAR, dup(l, buf));
				}

				chn(l);
				if (chp(l) != '\'') {
					ret = token(l, TOK_ERR, "' expected");
				}
			}

			else if (isalpha(chp(l)) || chp(l) == '_') {
				for (size_t i = 0; i < ARR_LEN(keywordPairs); i++) {
					const char *kp = keywordPairs[i].id;
					if (is_tok(l, kp)) {
						ret = token(l, keywordPairs[i].kind, (char*)kp);
						for (size_t i = 0; i < strlen(kp)-1; i++) chn(l);
						goto exit;
					}
				}

				ret = token(l, TOK_ID, get_word(l));
			}

			else ret = token(l, TOK_ERR, "unknown token");
		} break;
	}

exit:
	chn(l);
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

void lexer_free(Lexer *l) {
	free(l->memory);
}
