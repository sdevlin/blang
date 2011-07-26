#include <stdio.h>
#include "ast.h"

static void reduce_decl(struct decl *);
static void reduce_stmt(struct stmt *);
static void reduce_expr(struct expr **);

void ast_reduce(struct prog *prog, struct config *cfg)
{
	reduce_decl(prog->ast);
}

void reduce_decl(struct decl *d)
{
	if (!d) {
		return;
	}
	
	reduce_expr(&d->value);
	reduce_stmt(d->code);
	reduce_decl(d->next);
}

void reduce_stmt(struct stmt *s)
{
	if (!s) {
		return;
	}
	
	reduce_decl(s->decl);
	reduce_expr(&s->expr);
	reduce_stmt(s->body);
	reduce_stmt(s->ebody);
	reduce_stmt(s->next);
}

#define REDUCE(op, opk, newk) do { \
	if (e->left->kind != opk || e->right->kind != opk) break; \
	e->constant = e->left->constant op e->right->constant; \
	e->kind = newk; \
	expr_free(&e->left); \
	expr_free(&e->right); \
	return; } while (0)
#define REDUCE_CMP(op) REDUCE(op, EXPR_INT, EXPR_BOOLEAN)
#define REDUCE_ARITH(op) REDUCE(op, EXPR_INT, EXPR_INT)
#define REDUCE_BOOLEAN(op) REDUCE(op, EXPR_BOOLEAN, EXPR_BOOLEAN)

#define REDUCE_SELF(k, v) do { \
	if (e->left->kind == EXPR_NAME && \
	e->right->kind == EXPR_NAME && \
	e->left->symbol == e->right->symbol) { \
	e->constant = v; \
	e->kind = k; \
	expr_free(&e->left); \
	expr_free(&e->right); \
	return; } } while (0)
#define REDUCE_CMP_SELF(v) REDUCE_SELF(EXPR_BOOLEAN, v)
#define REDUCE_ARITH_SELF(v) REDUCE_SELF(EXPR_INT, v)
#define REDUCE_BOOLEAN_SELF() do { \
	if (e->left->kind == EXPR_NAME && \
	e->right->kind == EXPR_NAME && \
	e->left->symbol == e->right->symbol) { \
	*ep = expr_copy(e->right); \
	expr_free(&e); \
	return; } } while (0)

#define REDUCE_SHORT(a, b, k, v) do { \
	if (a->kind == k && a->constant == v && \
	!expr_has_effects(b)) { \
	e->kind = k; \
	e->constant = v; \
	expr_free(&e->left); \
	expr_free(&e->right); \
	return; } } while (0)
#define REDUCE_ARITH_SHORT(a, b, v) REDUCE_SHORT(a, b, EXPR_INT, v)
#define REDUCE_BOOLEAN_SHORT(a, b, v) REDUCE_SHORT(a, b, EXPR_BOOLEAN, v)

#define REDUCE_ID(a, b, k, v) do { \
	if (a->kind == k && a->constant == v) { \
	*ep = expr_copy(b); \
	expr_free(&e); \
	return; } } while (0)
#define REDUCE_ARITH_ID(a, b, v) REDUCE_ID(a, b, EXPR_INT, v)
#define REDUCE_BOOLEAN_ID(a, b, v) REDUCE_ID(a, b, EXPR_BOOLEAN, v)

#define REDUCE_UNARY(op, k) do { \
	if (e->right->kind != k) break; \
	e->constant = op e->right->constant; \
	e->kind = k; \
	expr_free(&e->right); \
	return; } while (0)

void reduce_expr(struct expr **ep)
{
	if (!ep || !(*ep)) {
		return;
	}
	
	struct expr *e = *ep;
	
	reduce_expr(&e->left);
	reduce_expr(&e->right);
	
	switch (e->kind) {
	case EXPR_LE:
		REDUCE_CMP(<=);
		REDUCE_CMP_SELF(1);
		break;
	case EXPR_LT:
		REDUCE_CMP(<);
		REDUCE_CMP_SELF(0);
		break;
	case EXPR_EQ:
		REDUCE_CMP(==);
		REDUCE_CMP_SELF(1);
		break;
	case EXPR_NE:
		REDUCE_CMP(!=);
		REDUCE_CMP_SELF(0);
		break;
	case EXPR_GT:
		REDUCE_CMP(>);
		REDUCE_CMP_SELF(0);
		break;
	case EXPR_GE:
		REDUCE_CMP(>=);
		REDUCE_CMP_SELF(1);
		break;
	case EXPR_ADD:
		REDUCE_ARITH(+);
		REDUCE_ARITH_ID(e->left, e->right, 0);
		REDUCE_ARITH_ID(e->right, e->left, 0);
		break;
	case EXPR_SUB:
		REDUCE_ARITH(-);
		REDUCE_ARITH_SELF(0);
		REDUCE_ARITH_ID(e->right, e->left, 0);
		break;
	case EXPR_MUL:
		REDUCE_ARITH(*);
		REDUCE_ARITH_SHORT(e->left, e->right, 0);
		REDUCE_ARITH_SHORT(e->right, e->left, 0);
		REDUCE_ARITH_ID(e->left, e->right, 1);
		REDUCE_ARITH_ID(e->right, e->left, 1);
		break;
	case EXPR_DIV:
		REDUCE_ARITH(/);
		REDUCE_ARITH_SELF(1);
		REDUCE_ARITH_SHORT(e->left, e->right, 0);
		REDUCE_ARITH_ID(e->right, e->left, 1);
		break;
	case EXPR_MOD:
		REDUCE_ARITH(%);
		REDUCE_ARITH_SELF(0);
		REDUCE_ARITH_SHORT(e->left, e->right, 0);
		if (e->right->kind == EXPR_INT && e->right->constant == 1 &&
		    !expr_has_effects(e->left)) {
			e->kind = EXPR_INT;
			e->constant = 0;
			expr_free(&e->left);
			expr_free(&e->right);
			return;
		}
		break;
	case EXPR_AND:
		REDUCE_BOOLEAN(&&);
		REDUCE_BOOLEAN_SHORT(e->left, e->right, 0);
		REDUCE_BOOLEAN_SHORT(e->right, e->left, 0);
		REDUCE_BOOLEAN_ID(e->left, e->right, 1);
		REDUCE_BOOLEAN_ID(e->right, e->left, 1);
		REDUCE_BOOLEAN_SELF();
		break;
	case EXPR_OR:
		REDUCE_BOOLEAN(||);
		REDUCE_BOOLEAN_SHORT(e->left, e->right, 1);
		REDUCE_BOOLEAN_SHORT(e->right, e->left, 1);
		REDUCE_BOOLEAN_ID(e->left, e->right, 0);
		REDUCE_BOOLEAN_ID(e->right, e->left, 0);
		REDUCE_BOOLEAN_SELF();
		break;
	case EXPR_NOT:
		REDUCE_UNARY(!, EXPR_BOOLEAN);
		break;
	case EXPR_POS:
		REDUCE_UNARY(+, EXPR_INT);
		break;
	case EXPR_NEG:
		REDUCE_UNARY(-, EXPR_INT);
		break;
	case EXPR_POW:
		if (e->right->kind == EXPR_INT && e->right->constant == 0 &&
		    !expr_has_effects(e->left)) {
			e->kind = EXPR_INT;
			e->constant = 1;
			expr_free(&e->left);
			expr_free(&e->right);
			return;
		}
		if (e->left->kind != EXPR_INT || e->right->kind != EXPR_INT) {
			break;
		}
		e->constant = 1;
		while (e->right->constant > 0) {
			e->constant *= e->left->constant;
			e->right->constant -= 1;
		}
		e->kind = EXPR_INT;
		expr_free(&e->left);
		expr_free(&e->right);
		break;
	case EXPR_ASSIGN:
		if (e->right->kind == EXPR_NAME && e->symbol == e->right->symbol) {
			*ep = expr_copy(e->right);
			expr_free(&e);
			return;
		}
		break;
	default:
		break;
	}
}
