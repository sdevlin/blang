#include <stdlib.h>
#include <stdio.h>
#include "ast.h"

static void alloc_decl(struct decl *);
static void alloc_stmt(struct stmt *);
static void alloc_expr(struct expr *);

static FILE *fout;
static FILE *ferr;

void ast_alloc(struct prog *prog, struct config *cfg)
{
	fout = cfg->fout;
	ferr = cfg->ferr;
	alloc_decl(prog->ast);
}

static struct decl *func;

static enum reg reg_alloc(void);
static void reg_free(enum reg);

void alloc_decl(struct decl *d)
{
	if (!d) {
		return;
	}
	
	switch (d->symbol->kind) {
	case SYMBOL_GLOBAL:
		if (d->type->kind == TYPE_FUNCTION) {
			func = d;
			alloc_stmt(d->code);
		}
		break;
	case SYMBOL_PARAM:
		break;
	case SYMBOL_LOCAL:
		if (d->value) {
			alloc_expr(d->value);
			reg_free(d->value->reg);
		}
		break;
	}
	
	alloc_decl(d->next);
}

void alloc_stmt(struct stmt *s)
{
	if (!s) {
		return;
	}
	
	alloc_decl(s->decl);
	alloc_expr(s->expr);
	if (s->expr && s->expr->reg > 0) {
		reg_free(s->expr->reg);
	}
	alloc_stmt(s->body);
	alloc_stmt(s->ebody);
	alloc_stmt(s->next);
}

void alloc_expr(struct expr *e)
{
	if (!e) {
		return;
	}
	
	alloc_expr(e->left);
	alloc_expr(e->right);
	
	switch (e->kind) {
	case EXPR_LE:
	case EXPR_LT:
	case EXPR_EQ:
	case EXPR_NE:
	case EXPR_GT:
	case EXPR_GE:
	case EXPR_AND:
	case EXPR_OR:
	case EXPR_ADD:
	case EXPR_SUB:
	case EXPR_MUL:
	case EXPR_POW:
		e->reg = e->left->reg;
		reg_free(e->right->reg);
		break;
	case EXPR_NOT:
	case EXPR_POS:
	case EXPR_NEG:
		e->reg = e->right->reg;
		break;
	case EXPR_DIV:
	case EXPR_MOD:
		if (e->right->reg == REG_EDX) {
			e->reg = e->left->reg;
			reg_free(e->right->reg);
		} else {		
			e->reg = e->right->reg;
			reg_free(e->left->reg);
		}
		break;
	case EXPR_PRE_INCR:
	case EXPR_PRE_DECR:
		e->reg = e->right->reg;
		break;
	case EXPR_POST_INCR:
	case EXPR_POST_DECR:
		e->reg = e->left->reg;
		break;
	case EXPR_INT:
	case EXPR_CHAR:
	case EXPR_BOOLEAN:
	case EXPR_STRING:
	case EXPR_NAME:
	case EXPR_CALL:
		e->reg = reg_alloc();
		func->regs |= e->reg;
		break;
	case EXPR_ASSIGN:
		e->reg = e->right->reg;
		break;
	case EXPR_ARG:
		reg_free(e->left->reg);
		break;
	default:
		return;
	}
}

static enum reg regs;

enum reg reg_alloc(void)
{
	enum reg reg;
	for (reg = REG_EBX; reg < REG_EAX; reg *= 2) {
		if (!(regs & reg)) {
			regs |= reg;
			return reg;
		}
	}
	fprintf(ferr, "alloc: cannot allocate register\n");
	exit(1);
}

void reg_free(enum reg reg)
{
	switch (reg) {
	case REG_EBX:
	case REG_ECX:
	case REG_EDX:
	case REG_ESI:
	case REG_EDI:
		if (!(regs & reg)) {
			fprintf(ferr, "alloc: attempted to free unallocated register '%s'\n", reg_to_s(reg));
			exit(1);
		}
		regs ^= reg;
		break;
	case REG_EAX:
	default:
		fprintf(ferr, "alloc: attempted to free out-of-range register %d\n", reg);
		exit(1);
		break;
	}
}
