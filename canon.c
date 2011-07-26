#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

static void canon_decl(struct decl *);
static void canon_stmt(struct stmt *);
static void canon_expr(struct expr *);

static struct prog *prog;

void ast_canon(struct prog *p, struct config *cfg)
{
	prog = p;
	canon_decl(prog->ast);
}

void canon_decl(struct decl *d)
{
	if (!d) {
		return;
	}
	
	canon_expr(d->value);
	canon_stmt(d->code);
	canon_decl(d->next);
	
	if (!d->value) {
		char *name;
		switch (d->type->kind) {
		case TYPE_INT:
			d->value = expr_make(EXPR_INT, NULL, NULL, NULL, 0);
			break;
		case TYPE_CHAR:
			d->value = expr_make(EXPR_CHAR, NULL, NULL, NULL, 0);
			break;
		case TYPE_BOOLEAN:
			d->value = expr_make(EXPR_BOOLEAN, NULL, NULL, NULL, 0);
			break;
		case TYPE_STRING:
			name = malloc(3);
			strcpy(name, "\"\"");
			d->value = expr_make(EXPR_STRING, NULL, NULL, name, 0);
			prog_add_string(prog, "\"\"");
			break;
		default:
			break;
		}
	}
}

#define WRAP(body) do { \
	if (!body || (body->kind != STMT_BLOCK)) { \
		body = stmt_make(STMT_BLOCK, NULL, NULL, body, NULL); \
	} } while (0)

void canon_stmt(struct stmt *s)
{
	if (!s) {
		return;
	}
	
	canon_decl(s->decl);
	canon_expr(s->expr);
	canon_stmt(s->body);
	canon_stmt(s->ebody);
	canon_stmt(s->next);
	
	switch (s->kind) {
	case STMT_WHILE:
		WRAP(s->body);
		break;
	case STMT_IF_ELSE:
		WRAP(s->body);
		WRAP(s->ebody);
		break;
	default:
		break;
	}
}

void canon_expr(struct expr *e)
{
	if (!e) {
		return;
	}
	
	canon_expr(e->left);
	canon_expr(e->right);
}
