#include <stdio.h>
#include "ast.h"

static void inline_decl(struct decl *);
static void inline_stmt(struct stmt **);
static void inline_expr(struct expr **);

void ast_inline(struct prog *prog, struct config *cfg)
{
	struct symbol *s = prog->symbols;
	while (s) {
		expr_free(&s->value);
		s = s->next;
	}
	inline_decl(prog->ast);
}

void inline_decl(struct decl *d)
{
	if (!d) {
		return;
	}
	
	inline_stmt(&d->code);
	inline_expr(&d->value);
	if (d->symbol->kind == SYMBOL_LOCAL &&
	    d->symbol->num_writes == 0 &&
	    expr_is_const(d->value)) {
		d->symbol->value = expr_copy(d->value);
	}
	
	inline_decl(d->next);
}

void inline_stmt(struct stmt **sp)
{
	if (!sp || !(*sp)) {
		return;
	}
	
	struct stmt *s = *sp;
	
	inline_decl(s->decl);
	inline_expr(&s->expr);
	inline_stmt(&s->body);
	inline_stmt(&s->ebody);
	inline_stmt(&s->next);
	
	if (s->kind == STMT_DECL &&
	    s->decl->symbol->value) {
		*sp = s->next;
		s->next = NULL;
		stmt_free(&s);
	}
}

void inline_expr(struct expr **ep)
{
	if (!ep || !(*ep)) {
		return;
	}
	
	struct expr *e = *ep;
	
	inline_expr(&e->left);
	inline_expr(&e->right);
	
	switch (e->kind) {
	case EXPR_NAME:
		if (e->symbol->value) {
			*ep = expr_copy(e->symbol->value);
			expr_free(&e);
		}
		break;
	default:
		break;
	}
}
