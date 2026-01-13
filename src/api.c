#include "../include/parser.h"
#include "../include/eval.h"
#include "../include/print.h"
#include "../include/api.h"

#define COPY(var_dst, src_var) \
	memcpy(var_dst, src_var, sizeof(*var_dst))

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

extern void reg_stdlib(EvalCtx *ctx);

EpslCtx *epsl_from_str(EpslErrorFn errf, char *code) {
	EpslCtxR *ctx = malloc(sizeof(EpslCtxR));
	ctx->parser = (Parser){
		.lexer = lexer_init("script", code),
		.err_ctx.errf = (ErrorFn) errf,
	};

	ctx->eval_ctx = (EvalCtx){
		.err_ctx.errf = (ErrorFn) errf,
		.stack = {0},
		.gc = {0},
	};

	reg_stdlib(&ctx->eval_ctx);
	return ctx;
}

void epsl_throw_error(EpslCtx *ctx, EpslLocation loc, char *msg) {
	EpslCtxR *r = ctx;
	r->eval_ctx.err_ctx.got_err = true;
	Location rloc; COPY(&rloc, &loc);
	r->eval_ctx.err_ctx.errf(rloc, ERROR_RUNTIME, msg);
}

EpslCtx *epsl_from_file(EpslErrorFn errf, char *filename) {
	char *code = read_file(filename);
	if (!code) return NULL;

	EpslCtxR *ctx = malloc(sizeof(EpslCtxR));
	ctx->parser = (Parser){
		.lexer = lexer_init(filename, code),
		.err_ctx.errf = (ErrorFn) errf,
	};

	ctx->eval_ctx = (EvalCtx){
		.err_ctx.errf = (ErrorFn) errf,
		.stack = {0},
		.gc = {0},
	};

	reg_stdlib(&ctx->eval_ctx);
	return ctx;
}

void epsl_reg_func(EpslCtx *ctx, const char *id, EpslRegFunc rf) {
	EpslCtxR *r = ctx;
	eval_reg_func(&r->eval_ctx, id, (RegFunc) rf);
}

void epsl_reg_var(EpslCtx *ctx, const char *id, EpslVal val) {
	EpslCtxR *r = ctx;
	Val ev; COPY(&ev, &val);
	eval_reg_var(&r->eval_ctx, id, ev);
}

EpslResult epsl_eval(EpslCtx *ctx) {
	EpslCtxR *r = ctx;
	AST *ast = parse(&r->parser);
	if (r->parser.err_ctx.got_err)
		return (EpslResult){.got_err = true};

	EpslVal erv;
	Val rv = eval(&r->eval_ctx, ast);
	if (r->eval_ctx.err_ctx.got_err)
		return (EpslResult){.got_err = true};

	memcpy(&erv, &rv, sizeof(rv));
	return (EpslResult){
		.val = erv,
		.got_err = false
	};
}

void epsl_print_ast(EpslCtx *ctx) {
	EpslCtxR *r = ctx;
	ast_print(parse(&r->parser), 0);
}

void epsl_print_tokens(EpslCtx *ctx) {
	EpslCtxR *r = ctx;
	lexer_print(r->parser.lexer);
}

EpslVal epsl_new_heap_val(EpslCtx *ctx, uint8_t kind) {
	EpslCtxR *r = ctx;
	Val val = eval_new_heap_val(&r->eval_ctx, kind);
	EpslVal ev; COPY(&ev, &val);
	return ev;
}

EpslString *epsl_val_get_str(EpslVal val) {
	Val v; COPY(&v, &val);
	return (EpslString*)VSTR(v);
}

void epsl_val_set_str(EpslCtx *ctx, EpslVal val, char *str) {
	Val v; COPY(&v, &val);
	StringBuilder *sb = VSTR(v);
	sb_reset(sb);
	sb_appendf(sb, "%s", str);
}

void epsl_list_append(EpslVals *list, EpslVal v) {
	da_append(list, v);
}
