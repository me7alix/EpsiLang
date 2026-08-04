#include "../include/parser.h"
#include "../include/eval.h"
#include "../include/print.h"
#include "../include/api.h"

#define VAR_FROM(T, var, src_var) \
	T var; memcpy(&(var), (src_var), sizeof(T))

typedef struct {
	Parser parser;
	EvalCtx eval_ctx;
} EpslCtxR;

extern void reg_stdlib(EvalCtx *ctx);

EpslCtx *epsl_from_str(EpslErrorFn errf, char *code) {
	EpslCtxR *ctx = malloc(sizeof(EpslCtxR));
	ctx->parser = (Parser){
		.lexer = lexer_from_str(&ctx->parser.arena, "script", code),
		.err_ctx.errf = (ErrorFn) errf,
	};
	ctx->eval_ctx = (EvalCtx){.err_ctx.errf = (ErrorFn) errf};
	reg_stdlib(&ctx->eval_ctx);
	return ctx;
}

EpslCtx *epsl_from_file(EpslErrorFn errf, char *filename) {
	EpslCtxR *ctx = malloc(sizeof(EpslCtxR));
	ctx->parser = (Parser){.err_ctx.errf = (ErrorFn) errf};
	Lexer lex = lexer_from_file(&ctx->parser.arena, filename);
	if (!lex.stream) return NULL;
	ctx->parser.lexer = lex;
	ctx->eval_ctx = (EvalCtx){.err_ctx.errf = (ErrorFn) errf};
	reg_stdlib(&ctx->eval_ctx);
	return ctx;
}

void epsl_throw_error(EpslEvalCtx *ctx, EpslLocation loc, char *msg) {
	EvalCtx *r = (EvalCtx*) ctx;
	VAR_FROM(Location, rloc, &loc);
	r->err_ctx.got_err = true;
	r->err_ctx.errf(&r->call_stack, rloc, ERROR_RUNTIME, msg);
}

void epsl_reg_func(EpslCtx *ctx, const char *id, EpslRegFunc rf) {
	EpslCtxR *r = ctx;
	eval_reg_func(&r->eval_ctx, id, (RegFunc) rf);
}

void epsl_reg_var(EpslCtx *ctx, bool is_const, const char *id, EpslVal val) {
	EpslCtxR *r = ctx;
	VAR_FROM(Val, ev, &val);
	eval_reg_var(&r->eval_ctx, is_const, id, ev);
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

EpslVal epsl_eval_make_value(EpslEvalCtx *ctx, int kind) {
	EvalCtx *e = (EvalCtx*) ctx;
	Val val = eval_make_val(e, kind);
	VAR_FROM(EpslVal, ev, &val);
	return ev;
}

EpslVal epsl_make_value(EpslCtx *ctx, int kind) {
	EpslCtxR *r = (EpslCtx*) ctx;
	Val val = eval_make_val(&r->eval_ctx, kind);
	VAR_FROM(EpslVal, ev, &val);
	return ev;
}

EpslVal epsl_eval_make_custom(EpslEvalCtx *ctx, EpslCustomObj custom) {
	EvalCtx *e = (EvalCtx*) ctx;
	VAR_FROM(EvalCustomObj, ec, &custom);
	Val val = eval_gc_new_custom(e, ec);
	VAR_FROM(EpslVal, ev, &val);
	return ev;
}

EpslVal epsl_make_custom(EpslCtx *ctx, EpslCustomObj custom) {
	EpslCtxR *r = (EpslCtxR*) ctx;
	VAR_FROM(EvalCustomObj, ec, &custom);
	Val val = eval_gc_new_custom(&r->eval_ctx, ec);
	VAR_FROM(EpslVal, ev, &val);
	return ev;
}

EpslString *epsl_val_get_str(EpslVal val) {
	VAR_FROM(Val, v, &val);
	return (EpslString*)VSTR(v);
}

void epsl_val_set_str(EpslVal val, char *str) {
	VAR_FROM(Val, v, &val);
	StringBuilder *sb = VSTR(v);
	sb_reset(sb);
	sb_appendf(sb, "%s", str);
}

void epsl_val_list_append(EpslVal list, EpslVal v) {
	VAR_FROM(Val, rl, &list);
	VAR_FROM(Val, rv, &v);
	da_append(VLIST(rl), rv);
}

EpslCustomObj epsl_val_get_custom(EpslVal v) {
	VAR_FROM(Val, val, &v);
	EvalCustomObj *eval_custom = val.as.gc_obj->data;
	VAR_FROM(EpslCustomObj, epsl_custom, eval_custom);
	return epsl_custom;
}

void epsl_free(EpslCtx *ctx) {
	EpslCtxR *rctx = (void*)ctx;
	parser_free(&rctx->parser);
	eval_free(&rctx->eval_ctx);
}
