#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "hash_table.h"

static void resolve_decl(struct decl *);
static void resolve_stmt(struct stmt *);
static void resolve_expr(struct expr *);
static void resolve_param(struct param *);

static struct prog *prog;
static FILE *fout;
static FILE *ferr;
static int should_print;

void ast_resolve(struct prog *p, struct config *cfg)
{
	prog = p;
	fout = cfg->fout;
	ferr = cfg->ferr;
	should_print = cfg->flags & FLAG_PRINT_RESOLVE;
	resolve_decl(p->ast);
}

static void resolve_print(struct symbol *s)
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
	fprintf(fout, "%s resolves to %s %d\n", s->name, kind, s->which);
}

static int scope_enter(void);
static int scope_level(void);
static int scope_exit(void);
static int scope_bind(char *name, struct symbol *symbol);
static struct symbol *scope_lookup(const char *name);

static int param_count;
static int local_count;

/*
not sure this is entirely right. this code allows all declarations at innermore levels to "shadow" 
declarations at outermore levels. this is similar to C, but not exactly the same. the main 
difference i'm aware of is that function locals should not be able to shadow function params. 
function locals with the same name as formal parameters should cause an error.
*/
void resolve_decl(struct decl *d)
{
	if (!d) {
		return;
	}
	
	struct type *t = d->type;
	if (scope_level() == SYMBOL_GLOBAL) {
		d->symbol = scope_lookup(d->name);
		if (!d->symbol) {
			d->symbol = symbol_make(SYMBOL_GLOBAL, t, d->name, prog);
			scope_bind(d->name, d->symbol);
		}
		int init = t->kind == TYPE_FUNCTION
			? d->code != NULL
			: d->value != NULL;
		if (init) {
			if (d->symbol->init) {
				fprintf(ferr, "resolve: redefinition of global %s\n", d->name);
				exit(1);
			}
			d->symbol->init = 1;
		}
		resolve_print(d->symbol);
		if (t->kind == TYPE_FUNCTION) {
			scope_enter();
			param_count = 0;
			resolve_param(t->params);
			local_count = 0;
			resolve_stmt(d->code);
			d->num_locals = local_count;
			scope_exit();
		}
	} else {
		d->symbol = symbol_make(SYMBOL_LOCAL, t, d->name, prog);
		d->symbol->offset = local_count++;
		if (!scope_bind(d->name, d->symbol)) {
			fprintf(ferr, "resolve: local %s has already been declared\n", d->name);
			exit(1);
		}
		resolve_print(d->symbol);
		resolve_expr(d->value);
	}
	
	resolve_decl(d->next);
}

void resolve_stmt(struct stmt *s)
{
	if (!s) {
		return;
	}
	
	switch (s->kind) {
	case STMT_DECL:
		resolve_decl(s->decl);
		break;
	case STMT_EXPR:
	case STMT_PRINT:
	case STMT_RETURN:
		resolve_expr(s->expr);
		break;
	case STMT_IF_ELSE:
		resolve_expr(s->expr);
		scope_enter();
		resolve_stmt(s->body);
		scope_exit();
		scope_enter();
		resolve_stmt(s->ebody);
		scope_exit();
		break;
	case STMT_WHILE:
		resolve_expr(s->expr);
		scope_enter();
		resolve_stmt(s->body);
		scope_exit();
		break;
	case STMT_BLOCK:
		scope_enter();
		resolve_stmt(s->body);
		scope_exit();
		break;
	}
	
	resolve_stmt(s->next);
}

void resolve_expr(struct expr *e)
{
	if (!e) {
		return;
	}
	
	switch (e->kind) {
	case EXPR_NAME:
	case EXPR_CALL:
	case EXPR_ASSIGN:
		e->symbol = scope_lookup(e->name);
		if (e->symbol) {
			resolve_print(e->symbol);
		} else {
			fprintf(ferr, "resolve: use of undeclared variable %s\n", e->name);
			exit(1);
		}
		resolve_expr(e->right); /* no-op for EXPR_NAME */
		break;
	case EXPR_STRING:
		prog_add_string(prog, e->name);
		break;
	default:
		resolve_expr(e->left);
		resolve_expr(e->right);
		break;
	}
}

void resolve_param(struct param *p)
{
	if (!p) {
		return;
	}
	
	struct symbol *s = symbol_make(SYMBOL_PARAM, p->type, p->name, prog);
	scope_bind(p->name, s);
	s->offset = param_count++;
	resolve_print(s);
	
	resolve_param(p->next);
}

#define SCOPE_MAX 100
static int level;
static struct hash_table *scope[SCOPE_MAX];
static int which;

int scope_enter(void)
{
	++level;
	if (level >= SCOPE_MAX) {
		fprintf(ferr, "resolve: max scope exceeded\n");
		exit(1);
	}
	scope[level] = hash_table_create(0, 0);
	return level;
}

int scope_level(void)
{
	return level;
}

int scope_exit(void)
{
	hash_table_delete(scope[level]);
	--level;
	return level;
}

static void scope_init(void)
{
	if (!scope[0]) {
		scope[0] = hash_table_create(0, 0);
	}
}

int scope_bind(char *name, struct symbol *s)
{
	scope_init();
	
	if (hash_table_lookup(scope[level], name)) {
		return 0;
	} else {
		s->which = ++which;
		hash_table_insert(scope[level], name, s, NULL);
		return 1;
	}
}

struct symbol *scope_lookup(const char *name)
{
	scope_init();
	
	int i;
	struct symbol *s;
	for (i = level; i >= 0; --i) {
		s = hash_table_lookup(scope[i], name);
		if (s) {
			return s;
		}
	}
	return NULL;
}
