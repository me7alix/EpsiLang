#ifndef PRINT_H
#define PRINT_H

#include "../include/lexer.h"
#include "../include/parser.h"

void lexer_print(Lexer l);
void ast_print(AST *n, int spaces);

#endif
