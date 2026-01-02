#include "../include/parser.h"
#include "../include/eval.h"
#include "../include/api.h"
#include "../include/print.h"

char *read_file(const char *filename) {
	FILE *file = fopen(filename, "rb");
	if (!file) return NULL;

	fseek(file, 0, SEEK_END);
	long filesize = ftell(file);
	rewind(file);

	char *buffer = malloc(filesize + 1);
	size_t read_size = fread(buffer, 1, filesize, file);
	if (read_size != filesize) {
		free(buffer);
		fclose(file);
		return NULL;
	}

	buffer[filesize] = '\0';
	fclose(file);

	return buffer;
}

typedef struct {
	Parser parser;
	EvalCtx eval_ctx;
} EpslCtxR;

extern void reg_stdlib(Parser *p, EvalCtx *ctx);

EpslCtx *epsl_from_str(char *code) {
	EpslCtxR *ctx = malloc(sizeof(EpslCtxR));
	ctx->parser = (Parser){lexer_init("script", code)};
	ctx->eval_ctx = (EvalCtx){0};
	return ctx;
}

EpslCtx *epsl_from_file(char *filename) {
	char *code = read_file(filename);
	if (!code) return NULL;

	EpslCtxR *ctx = malloc(sizeof(EpslCtxR));
	ctx->parser = (Parser){lexer_init(filename, code)};
	ctx->eval_ctx = (EvalCtx){0};
	reg_stdlib(&ctx->parser, &ctx->eval_ctx);
	return ctx;
}

void epsl_reg_func(EpslCtx *ctx, EpslRegFunc rf, char *name,
		EpslFuncArgsKind fk, size_t cnt) {
	EpslCtxR *rctx = ctx;
	reg_func(&rctx->parser, &rctx->eval_ctx, (RegFunc)rf, name, (int)fk, cnt);
}

EpslVal epsl_eval(EpslCtx *ctx) {
	EpslCtxR *rctx = ctx;
	AST *ast = parse(&rctx->parser);
	Val rv = eval(&rctx->eval_ctx, ast);

	EpslVal erv;
	memcpy(&erv, &rv, sizeof(rv));
	return erv;
}

void epsl_print_ast(EpslCtx *ctx) {
	EpslCtxR *rctx = ctx;
	ast_print(parse(&rctx->parser), 0);
}

void epsl_print_tokens(EpslCtx *ctx) {
	EpslCtxR *rctx = ctx;
	ast_print(parse(&rctx->parser), 0);
}
