#include <stdlib.h>
#include "ast.h"

static void typecheck_decl(struct decl *);
static void typecheck_stmt(struct stmt *);
static void typecheck_expr(struct expr *);

static FILE *ferr;

void ast_typecheck(struct prog *prog, struct config *cfg)
{
	ferr = cfg->ferr;
	typecheck_decl(prog->ast);
}

static enum type_kind ftype_kind;

void typecheck_decl(struct decl *d)
{
	if (!d) {
		return;
	}
	
	typecheck_expr(d->value);
	
	if (d->type->kind == TYPE_FUNCTION) {
		struct type *t1 = d->type;
		struct type *t2 = d->symbol->type;
		if (t2->kind != TYPE_FUNCTION ||
		    t1->rtype->kind != t2->rtype->kind) {
			fprintf(ferr, "typecheck: function '%s' conflicting return types\n", d->name);
			exit(1);
		}
		if (t1->rtype->kind == TYPE_UNKNOWN) {
			fprintf(ferr, "typecheck: function '%s' return type cannot  be inferred\n", d->name);
			exit(1);
		}
		struct param *p1 = t1->params;
		struct param *p2 = t2->params;
		while (p1 && p2) {
			if (p1->type->kind != p2->type->kind) {
				fprintf(ferr, "typecheck: function '%s' param list type mismatch\n", d->name);
				exit(1);
			}
			if (p1->type->kind == TYPE_UNKNOWN) {
				fprintf(ferr, "typecheck: function '%s' parameter type cannot be inferred\n", d->name);
				exit(1);
			}
			p1 = p1->next;
			p2 = p2->next;
		}
		if (p1 || p2) {
			fprintf(ferr, "typecheck: function '%s' param list count mismatch\n", d->name);
			exit(1);
		}
		ftype_kind = t1->rtype->kind;
	} else {
		int kind = expr_to_type_kind(d->value);
		switch (d->type->kind) {
		case TYPE_UNKNOWN:
			if (kind == TYPE_UNKNOWN) {
				fprintf(ferr, "typecheck: cannot infer type of uninitialized variable\n");
				exit(1);
			}
			d->type->kind = kind;
			break;
		case TYPE_VOID:
			fprintf(ferr, "typecheck: variables cannot be of type void\n");
			exit(1);
			break;
		default:
			if (kind != TYPE_UNKNOWN && 
			    kind != d->type->kind) {
				fprintf(ferr, "typecheck: cannot assign %s to %s\n",
				        type_kind_to_s(kind), type_kind_to_s(d->type->kind));
				exit(1);
			}
			if (d->symbol->kind == SYMBOL_GLOBAL &&
			    !expr_is_const(d->value)) {
				fprintf(ferr, "typecheck: global '%s' initializer must be constant\n", d->name);
				exit(1);
			}
			break;
		}
	}
	
	typecheck_stmt(d->code);
	typecheck_decl(d->next);
}

void typecheck_stmt(struct stmt *s)
{
	if (!s) {
		return;
	}
	
	typecheck_expr(s->expr);
	
	switch (s->kind) {
	case STMT_IF_ELSE:
	case STMT_WHILE:
		if (expr_to_type_kind(s->expr) != TYPE_BOOLEAN) {
			fprintf(ferr, "typecheck: condition must be a boolean expr\n");
			exit(1);
		}
		break;
	case STMT_RETURN:
		if (expr_to_type_kind(s->expr) != ftype_kind) {
			fprintf(ferr, "typecheck: type of expr in return statement must match function return type\n");
			exit(1);
		}
		break;
	default:
		break;
	}
	
	typecheck_decl(s->decl);
	typecheck_stmt(s->body);
	typecheck_stmt(s->ebody);
	typecheck_stmt(s->next);
}

void typecheck_expr(struct expr *e)
{
	if (!e) {
		return;
	}
	
	typecheck_expr(e->left);
	typecheck_expr(e->right);
	
	struct expr *arg;
	struct param *param;
	switch (e->kind) {
	case EXPR_LT:
	case EXPR_LE:
	case EXPR_GT:
	case EXPR_GE:
	case EXPR_ADD:
	case EXPR_SUB:
	case EXPR_MUL:
	case EXPR_DIV:
	case EXPR_MOD:
	case EXPR_POW:
	case EXPR_PRE_INCR:
	case EXPR_PRE_DECR:
	case EXPR_POST_INCR:
	case EXPR_POST_DECR:
	case EXPR_POS:
	case EXPR_NEG:
		if ((e->left && expr_to_type_kind(e->left) != TYPE_INT) ||
		    (e->right && expr_to_type_kind(e->right) != TYPE_INT)) {
			fprintf(ferr, "typecheck: operator requires integral operand(s)\n");
			exit(1);
		}
		break;
	case EXPR_EQ:
	case EXPR_NE:
		if (expr_to_type_kind(e->left) != expr_to_type_kind(e->right)) {
			fprintf(ferr, "typecheck: operator requires like operands\n");
			exit(1);
		}
		break;
	case EXPR_ASSIGN:
		if (e->symbol->type->kind != expr_to_type_kind(e->right)) {
			fprintf(ferr, "typecheck: operator requires like operands\n");
			exit(1);
		}
		break;
	case EXPR_NOT:
	case EXPR_AND:
	case EXPR_OR:
		if ((e->left && expr_to_type_kind(e->left) != TYPE_BOOLEAN) ||
		    (expr_to_type_kind(e->right) != TYPE_BOOLEAN)) {
			fprintf(ferr, "typecheck: this operator requires boolean operand(s)\n");
			exit(1);
		}
		break;
	case EXPR_CALL:
		if (e->symbol->type->kind != TYPE_FUNCTION) {
			fprintf(ferr, "typecheck: called object '%s' is not a function\n", e->name);
			exit(1);
		}
		arg = e->right;
		param = e->symbol->type->params;
		while (arg && param) {
			if (expr_to_type_kind(arg->left) != param->type->kind) {
				fprintf(ferr, "typecheck: incorrect argument type in call to '%s'\n", e->name);
				exit(1);
			}
			arg = arg->right;
			param = param->next;
		}
		if (arg || param) {
			fprintf(ferr, "typecheck: incorrect number of arguments in call to '%s'\n", e->name);
			exit(1);
		}
		break;
	default:
		break;
	}
}
