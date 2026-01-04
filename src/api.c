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

EpslCtx *epsl_from_str(EpslErrorFn errf, char *code) {
	EpslCtxR *ctx = malloc(sizeof(EpslCtxR));
	ctx->parser = (Parser){
		.lexer = lexer_init("script", code),
		.ec.errf = (ErrorFn) errf,
	};

	ctx->eval_ctx = (EvalCtx){
		.ec.errf = (ErrorFn) errf,
		.stack = {0},
	};

	reg_stdlib(&ctx->parser, &ctx->eval_ctx);
	return ctx;
}

EpslCtx *epsl_from_file(EpslErrorFn errf, char *filename) {
	char *code = read_file(filename);
	if (!code) return NULL;

	EpslCtxR *ctx = malloc(sizeof(EpslCtxR));
	ctx->parser = (Parser){
		.lexer = lexer_init(filename, code),
		.ec.errf = (ErrorFn) errf,
	};

	ctx->eval_ctx = (EvalCtx){
		.ec.errf = (ErrorFn) errf,
		.stack = {0},
	};

	reg_stdlib(&ctx->parser, &ctx->eval_ctx);
	return ctx;
}

void epsl_reg_func(EpslCtx *ctx, const char *id, EpslRegFunc rf) {
	EpslCtxR *rctx = ctx;
	reg_func(&rctx->eval_ctx, id, (RegFunc) rf);
}

void epsl_reg_var(EpslCtx *ctx, const char *id, EpslVal val) {
	EpslCtxR *rctx = ctx;
	Val ev; memcpy(&ev, &val, sizeof(ev));
	reg_var(&rctx->eval_ctx, id, ev);
}

EpslResult epsl_eval(EpslCtx *ctx) {
	EpslCtxR *rctx = ctx;
	AST *ast = parse(&rctx->parser);
	if (rctx->parser.ec.got_err)
		return (EpslResult){.got_err = true};

	EpslVal erv;
	Val rv = eval(&rctx->eval_ctx, ast);
	if (rctx->eval_ctx.ec.got_err)
		return (EpslResult){.got_err = true};

	memcpy(&erv, &rv, sizeof(rv));
	return (EpslResult){
		.val = erv,
		.got_err = true
	};
}

void epsl_print_ast(EpslCtx *ctx) {
	EpslCtxR *rctx = ctx;
	ast_print(parse(&rctx->parser), 0);
}

void epsl_print_tokens(EpslCtx *ctx) {
	EpslCtxR *rctx = ctx;
	lexer_print(rctx->parser.lexer);
}
