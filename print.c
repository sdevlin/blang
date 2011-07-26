#include <stdio.h>
#include <stdarg.h>
#include "ast.h"

static void print_decl(struct decl *);
static void print_stmt(struct stmt *);
static void print_expr(struct expr *);
static void print_type(struct type *);
static void print_param(struct param *);

static FILE *fout;

static void write(const char *fmt, ...)
{
	va_list argp;
	va_start(argp, fmt);
	vfprintf(fout, fmt, argp);
	fputc('\n', fout);
	va_end(argp);
}

void ast_print(struct prog *prog, struct config *cfg)
{
	fout = cfg->fout;
	print_decl(prog->ast);
}

void print_decl(struct decl *d)
{
	if (!d) {
		return;
	}
	
	struct type *t = d->type;
	print_type(t);
	write("%s", d->name);
	
	if (t->kind == TYPE_FUNCTION) {
		write("(");
		print_param(t->params);
		write(")");
		if (d->code) {
			print_stmt(d->code);
		} else {
			write(";");
		}
	} else {
		if (d->value) {
			write("=");
			print_expr(d->value);
		}
		write(";");
	}
	
	print_decl(d->next);
}

void print_stmt(struct stmt *s)
{
	if (!s) {
		return;
	}
	
	switch (s->kind) {
	case STMT_DECL:
		print_decl(s->decl);
		break;
	case STMT_EXPR:
		print_expr(s->expr);
		write(";");
		break;
	case STMT_IF_ELSE:
		write("if\n(");
		print_expr(s->expr);
		write(")");
		print_stmt(s->body);
		if (s->ebody) {
			write("else");
			print_stmt(s->ebody);
		}
		break;
	case STMT_WHILE:
		write("while\n(");
		print_expr(s->expr);
		write(")");
		print_stmt(s->body);
		break;
	case STMT_RETURN:
		write("return");
		print_expr(s->expr);
		write(";");
		break;
	case STMT_BLOCK:
		write("{");
		print_stmt(s->body);
		write("}");
		break;
	case STMT_PRINT:
		write("print");
		print_expr(s->expr);
		write(";");
		break;
	}
	
	print_stmt(s->next);
}

void print_expr(struct expr *e)
{
	if (!e) {
		return;
	}
	
	const char *op = expr_kind_to_s(e->kind);
	
	print_expr(e->left);
	
	switch (e->kind) {
	case EXPR_CALL:
		write("%s\n(", e->name);
		print_expr(e->right); // special case, need to print right expr early here
		write(")");
		return;
	case EXPR_ARG:
		if (e->right) {
			write(",");
		}
		break;
	case EXPR_NAME:
	case EXPR_CHAR:
	case EXPR_STRING:
		write("%s", e->name);
		break;
	case EXPR_INT:
		write("%d", e->constant);
		break;
	case EXPR_BOOLEAN:
		if (e->constant) {
			write("true");
		} else {
			write("false");
		}
		break;
	case EXPR_ASSIGN:
		write("%s", e->name);
		write("%s", op);
		break;
	default:
		if (op) {
			write("%s", op);
		}
		break;
	}
	
	print_expr(e->right);
}

void print_type(struct type *t)
{
	if (!t) {
		return;
	}
	
	switch (t->kind) {
	case TYPE_FUNCTION:
		print_type(t->rtype);
		break;
	default:
		write("%s", type_kind_to_s(t->kind));
		break;
	}
}

void print_param(struct param *p)
{
	if (!p) {
		return;
	}
	
	print_type(p->type);
	write("%s", p->name);
	if (p->next) {
		write(",");
		print_param(p->next);
	}
}
