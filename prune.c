#include <stdio.h>
#include "ast.h"

static void prune_decl(struct decl **);
static void prune_stmt(struct stmt **);
static void prune_expr(struct expr **);

void ast_prune(struct prog *prog, struct config *cfg)
{
	prune_decl(&prog->ast);
}

void prune_decl(struct decl **dp)
{
	if (!dp || !(*dp)) {
		return;
	}
	
	struct decl *d = *dp;
	
	prune_decl(&d->next);
	
	prune_expr(&d->value);
	prune_stmt(&d->code);
	
	switch (d->symbol->kind) {
	case SYMBOL_LOCAL:
		/*if (d->symbol->num_writes == 0 &&
		    expr_is_const(d->value)) {
			d->symbol->value = expr_copy(d->value);
			decl_free(dp);
		}*/
		break;
	default:
		break;
	}
}

void prune_stmt(struct stmt **sp)
{
	if (!sp || !(*sp)) {
		return;
	}
	
	struct stmt *s = *sp;
	
	prune_stmt(&s->next);
	
	prune_decl(&s->decl);
	prune_expr(&s->expr);
	prune_stmt(&s->body);
	prune_stmt(&s->ebody);
	
	switch (s->kind) {
	case STMT_DECL:
		/* excise unread decl? */
		if (s->decl->symbol->num_reads == 0) {
			
		}
		break;
	case STMT_EXPR:
		if (!expr_has_effects(s->expr)) {
			*sp = s->next;
			s->next = NULL;
			stmt_free(&s);
		}
		break;
	case STMT_IF_ELSE:
		if (s->expr->kind == EXPR_BOOLEAN) {
			if (s->expr->constant == 1) {
				*sp = s->body;
				s->body = NULL;
				(*sp)->next = s->next;
			} else {
				*sp = s->ebody;
				s->ebody = NULL;
				(*sp)->next = s->next;
			}
			s->next = NULL;
			stmt_free(&s);
		}
		break;
	case STMT_WHILE:
		if (s->expr->kind == EXPR_BOOLEAN) {
			/* it would be really nice to replace the true cond
			   with a simple loop with no condition */
			if (s->expr->constant == 0) {
				*sp = s->next;
				s->next = NULL;
				stmt_free(&s);
			}
		}
		break;
	case STMT_RETURN:
		stmt_free(&s->next);
		break;
	default:
		break;
	}
}

void prune_expr(struct expr **ep)
{
	if (!ep || !(*ep)) {
		return;
	}
	
	struct expr *e = *ep;
	
	prune_expr(&e->left);
	prune_expr(&e->right);
	
	switch (e->kind) {
	case EXPR_ASSIGN:
		if (e->symbol->num_reads == 0) {
			*ep = e->right;
			e->right = NULL;
			expr_free(&e);
		}
		break;
	default:
		break;
	}
}
