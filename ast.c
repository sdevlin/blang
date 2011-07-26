#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "hash_table.h"

#define NEW(t) (malloc(sizeof(struct t)))

struct prog *prog_make(struct decl *ast)
{
	struct prog *p = NEW(prog);
	p->ast = ast;
	p->strings = hash_table_create(0, 0);
	p->symbols = NULL;
	return p;
}

void prog_add_string(struct prog *prog, const char *string)
{
	static int count = 0;
	int *ip = malloc(sizeof(int));
	*ip = ++count;
	hash_table_insert(prog->strings, string, ip, NULL);
}

void prog_free(struct prog **pp)
{
	if (!pp || !(*pp)) {
		return;
	}
	struct prog *p = *pp;
	decl_free(&p->ast);
	hash_table_delete(p->strings);
	symbol_free(&p->symbols);
	free(p);
	*pp = 0;
}

const char *reg_to_s(enum reg reg)
{
	switch (reg) {
	case REG_EAX:
		return "%eax";
	case REG_EBX:
		return "%ebx";
	case REG_ECX:
		return "%ecx";
	case REG_EDX:
		return "%edx";
	case REG_ESI:
		return "%esi";
	case REG_EDI:
		return "%edi";
	default:
		return NULL;
	}
}

struct decl *decl_make(char *name, struct type *type, struct expr *value, struct stmt *code)
{
	struct decl *d = NEW(decl);
	d->name = name;
	d->type = type;
	d->value = value;
	d->code = code;
	d->symbol = NULL;
	d->next = NULL;
	d->num_locals = 0;
	d->regs = 0;
	return d;
}

void decl_free(struct decl **dp)
{
	if (!dp || !(*dp)) {
		return;
	}
	struct decl *d = *dp;
	free(d->name);
	type_free(&d->type);
	expr_free(&d->value);
	stmt_free(&d->code);
	decl_free(&d->next);
	free(d);
	*dp = 0;
}

const char *type_kind_to_s(enum type_kind kind)
{
	switch (kind) {
	case TYPE_CHAR:
		return "char";
	case TYPE_INT:
		return "int";
	case TYPE_BOOLEAN:
		return "boolean";
	case TYPE_STRING:
		return "string";
	case TYPE_VOID:
		return "void";
	default:
		return NULL;
	}
}

struct type *type_make(enum type_kind kind, struct param *params, struct type *rtype)
{
	struct type *t = NEW(type);
	t->kind = kind;
	t->params = params;
	t->rtype = rtype;
	return t;
}

void type_free(struct type **tp)
{
	if (!tp || !(*tp)) {
		return;
	}
	struct type *t = *tp;
	param_free(&t->params);
	type_free(&t->rtype);
	free(t);
	*tp = 0;
}

const char *expr_kind_to_s(enum expr_kind kind)
{
	switch (kind) {
	case EXPR_LT:
		return "<";
	case EXPR_LE:
		return "<=";
	case EXPR_EQ:
		return "==";
	case EXPR_NE:
		return "!=";
	case EXPR_GT:
		return ">";
	case EXPR_GE:
		return ">=";
	case EXPR_NOT:
		return "!";
	case EXPR_ADD:
	case EXPR_POS:
		return "+";
	case EXPR_SUB:
	case EXPR_NEG:
		return "-";
	case EXPR_MUL:
		return "*";
	case EXPR_DIV:
		return "/";
	case EXPR_MOD:
		return "%";
	case EXPR_ASSIGN:
		return "=";
	case EXPR_PRE_INCR:
	case EXPR_POST_INCR:
		return "++";
	case EXPR_PRE_DECR:
	case EXPR_POST_DECR:
		return "--";
	case EXPR_OR:
		return "||";
	case EXPR_AND:
		return "&&";
	case EXPR_POW:
		return "^";
	default:
		return NULL;
	}
}

struct expr *expr_make(enum expr_kind kind, struct expr *left, struct expr *right, char *name, int constant)
{
	struct expr *e = NEW(expr);
	e->kind = kind;
	e->left = left;
	e->right = right;
	e->name = name; /* consider copying this */
	e->constant = constant;
	e->symbol = NULL;
	e->reg = 0;
	return e;
}

struct expr *expr_copy(struct expr *e)
{
	if (!e) {
		return NULL;
	}
	struct expr *copy = expr_make(e->kind, NULL, NULL, NULL, e->constant);
	if (e->name) {
		copy->name = malloc(strlen(e->name) + 1);
		strcpy(copy->name, e->name);
	}
	copy->left = expr_copy(e->left);
	copy->right = expr_copy(e->right);
	copy->symbol = e->symbol;
	return copy;
}

void expr_free(struct expr **ep)
{
	if (!ep || !(*ep)) {
		return;
	}
	struct expr *e = *ep;
	expr_free(&e->left);
	expr_free(&e->right);
	free(e->name);
	free(e);
	*ep = 0;
}

enum type_kind expr_to_type_kind(struct expr *e)
{
	if (!e) {
		return TYPE_UNKNOWN;
	}
	
	switch (e->kind) {
	case EXPR_LT:
	case EXPR_LE:
	case EXPR_EQ:
	case EXPR_NE:
	case EXPR_GT:
	case EXPR_GE:
	case EXPR_NOT:
	case EXPR_AND:
	case EXPR_OR:
	case EXPR_BOOLEAN:
		return TYPE_BOOLEAN;
	case EXPR_ADD:
	case EXPR_SUB:
	case EXPR_MUL:
	case EXPR_DIV:
	case EXPR_POS:
	case EXPR_NEG:
	case EXPR_MOD:
	case EXPR_PRE_INCR:
	case EXPR_PRE_DECR:
	case EXPR_POST_INCR:
	case EXPR_POST_DECR:
	case EXPR_POW:
	case EXPR_INT:
		return TYPE_INT;
	case EXPR_CHAR:
		return TYPE_CHAR;
	case EXPR_STRING:
		return TYPE_STRING;
	case EXPR_ARG:
		return expr_to_type_kind(e->left);
	case EXPR_ASSIGN:
	case EXPR_NAME:
		return e->symbol->type->kind;
	case EXPR_CALL:
		return e->symbol->type->rtype->kind;
	default:
		return TYPE_UNKNOWN;
	}
}

int expr_is_const(struct expr *e)
{
	if (!e) {
		return 1;
	}
	
	switch (e->kind) {
	case EXPR_INT:
	case EXPR_BOOLEAN:
	case EXPR_CHAR:
	case EXPR_STRING:
		return 1;
	case EXPR_ASSIGN:
	case EXPR_NAME:
	case EXPR_CALL:
		return 0;
	default:
		return expr_is_const(e->left) && expr_is_const(e->right);
	}
}

int expr_has_effects(struct expr *e)
{
	if (!e) {
		return 0;
	}
	
	switch (e->kind) {
	case EXPR_INT:
	case EXPR_BOOLEAN:
	case EXPR_CHAR:
	case EXPR_STRING:
		return 0;
	case EXPR_ASSIGN:
	case EXPR_CALL:
	case EXPR_PRE_INCR:
	case EXPR_PRE_DECR:
	case EXPR_POST_INCR:
	case EXPR_POST_DECR:
		return 1;
	default:
		return expr_has_effects(e->left) || expr_has_effects(e->right);
	}
}

struct stmt *stmt_make(enum stmt_kind kind, struct decl *decl, struct expr *expr, struct stmt *body, struct stmt *ebody)
{
	struct stmt *s = NEW(stmt);
	s->kind = kind;
	s->decl = decl;
	s->expr = expr;
	s->body = body;
	s->ebody = ebody;
	s->next = NULL;
	return s;
}

void stmt_free(struct stmt **sp)
{
	if (!sp || !(*sp)) {
		return;
	}
	struct stmt *s = *sp;
	decl_free(&s->decl);
	expr_free(&s->expr);
	stmt_free(&s->body);
	stmt_free(&s->ebody);
	stmt_free(&s->next);
	free(s);
	*sp = 0;
}

struct symbol *symbol_make(enum symbol_kind kind, struct type *type, char *name, struct prog *prog)
{
	struct symbol *s = NEW(symbol);
	s->kind = kind;
	s->which = 0;
	s->type = type;
	s->name = name;
	s->offset = 0;
	s->init = 0;
	s->value = NULL;
	s->num_reads = 0;
	s->num_writes = 0;
	if (prog) {
		s->next = prog->symbols;
		prog->symbols = s;
	}
	return s;
}

void symbol_free(struct symbol **sp)
{
	if (!sp || !(*sp)) {
		return;
	}
	struct symbol *s = *sp;
	type_free(&s->type);
	free(s->name);
	expr_free(&s->value);
	free(s->next);
	free(s);	
	*sp = 0;
}

struct param * param_make(char *name, struct type *type, struct param *next)
{
	struct param *p = NEW(param);
	p->name = name;
	p->type = type;
	p->next = next;
	return p;
}

void param_free(struct param **pp)
{
	if (!pp || !(*pp)) {
		return;
	}
	struct param *p = *pp;
	free(p->name);
	type_free(&p->type);
	param_free(&p->next);
	free(p);
	*pp = 0;
}
