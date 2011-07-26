#include <stdio.h>
#include <string.h>
#include "ast.h"

static void annotate_decl(struct decl *);
static void annotate_stmt(struct stmt *);
static void annotate_expr(struct expr *);

static FILE *fout;
static int should_print;

void ast_annotate(struct prog *prog, struct config *cfg)
{
	struct symbol *s = prog->symbols;
	while (s) {
		s->num_reads = 0;
		s->num_writes = 0;
		s = s->next;
	}
	fout = cfg->fout;
	should_print = cfg->flags & FLAG_PRINT_ANNOTATE;
	annotate_decl(prog->ast);
}

static void annotate_print(struct symbol *s)
{
	if (!should_print) {
		return;
	}
	
	char *kind;
	switch (s->kind) {
	case SYMBOL_LOCAL:
		kind = "local";
		break;
	case SYMBOL_PARAM:
		kind = "parameter";
		break;
	case SYMBOL_GLOBAL:
		kind = "global";
		break;
	}
	fprintf(fout, "%s %s read/write: %d/%d\n", kind, s->name, s->num_reads, s->num_writes);
}

void annotate_decl(struct decl *d)
{
	if (!d) {
		return;
	}
	
	annotate_stmt(d->code);
	annotate_expr(d->value);	
	annotate_decl(d->next);
}

void annotate_stmt(struct stmt *s)
{
	if (!s) {
		return;
	}
	
	annotate_decl(s->decl);
	annotate_expr(s->expr);
	annotate_stmt(s->body);
	annotate_stmt(s->ebody);	
	annotate_stmt(s->next);
}

void annotate_expr(struct expr *e)
{
	if (!e) {
		return;
	}
	
	annotate_expr(e->left);
	annotate_expr(e->right);
	
	switch (e->kind) {
	case EXPR_ASSIGN:
		++e->symbol->num_writes;
		annotate_print(e->symbol);
		break;
	case EXPR_CALL:
		++e->symbol->num_reads;
		annotate_print(e->symbol);
		break;
	case EXPR_PRE_INCR:
	case EXPR_PRE_DECR:
		++e->right->symbol->num_reads;
		++e->right->symbol->num_writes;
		annotate_print(e->right->symbol);
		break;
	case EXPR_POST_INCR:
	case EXPR_POST_DECR:
		++e->left->symbol->num_reads;
		++e->left->symbol->num_writes;
		annotate_print(e->left->symbol);
		break;
	default:
		if (e->left && e->left->kind == EXPR_NAME) {
			++e->left->symbol->num_reads;
			annotate_print(e->left->symbol);
		}
		if (e->right && e->right->kind == EXPR_NAME) {
			++e->right->symbol->num_reads;
			annotate_print(e->right->symbol);
		}
		return;
	}
}
