#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "ast.h"
#include "hash_table.h"

static void codegen_decl(struct decl *);
static void codegen_stmt(struct stmt *);
static void codegen_expr(struct expr *);

static struct hash_table *strings;
static FILE *fout;
static FILE *ferr;

static void write(const char *fmt, ...)
{
	va_list argp;
	va_start(argp, fmt);
	vfprintf(fout, fmt, argp);
	fputc('\n', fout);
	va_end(argp);
}

void ast_codegen(struct prog *prog, struct config *cfg)
{
	strings = prog->strings;
	fout = cfg->fout;
	ferr = cfg->ferr;
	write("\t.text");
	char *s;
	int *ip;
	hash_table_firstkey(strings);
	while (hash_table_nextkey(strings, &s, (void **)&ip)) {
		write(".string%d:", *ip);
		write("\t.string\t%s", s);
	}
	codegen_decl(prog->ast);
}

static char *func_name;
static void loc_from_symbol(char *buffer, struct symbol *);

#define MAYBE_PUSH(r) if (d->regs & r) write("\tpushl\t%s", reg_to_s(r))
#define MAYBE_POP(r) if (d->regs & r) write("\tpopl\t%s", reg_to_s(r))

void codegen_decl(struct decl *d)
{
	if (!d) {
		return;
	}
	
	char buffer[16];
	switch (d->symbol->kind) {
	case SYMBOL_GLOBAL:
		switch (d->type->kind) {
		case TYPE_FUNCTION:
			if (!d->code) {
				break;
			}
			write("\t.text");
			write(".globl %s", d->name);
			write("%s:", d->name);
			write("\tpushl\t%%ebp");
			write("\tmovl\t%%esp, %%ebp");
			if (d->num_locals > 0) {
				write("\tsubl\t$%d, %%esp", d->num_locals * 4);
			}
			MAYBE_PUSH(REG_EBX);
			MAYBE_PUSH(REG_ECX);
			MAYBE_PUSH(REG_EDX);
			MAYBE_PUSH(REG_ESI);
			MAYBE_PUSH(REG_EDI);
			func_name = d->name;
			codegen_stmt(d->code);
			write("\tmovl\t$0, %%eax");
			write(".%sret:", d->name);
			MAYBE_POP(REG_EDI);
			MAYBE_POP(REG_ESI);
			MAYBE_POP(REG_EDX);
			MAYBE_POP(REG_ECX);
			MAYBE_POP(REG_EBX);
			write("\tleave");
			write("\tret");
			break;
		default:
			write("\t.data");
			write(".globl %s", d->name);
			write("%s:", d->name);
			if (d->value) {
				if (d->type->kind == TYPE_STRING) {
					int *ip = hash_table_lookup(strings, d->value->name);
					sprintf(buffer, ".string%d", *ip);
				} else {
					sprintf(buffer, "%d", d->value->constant);
				}
			} else {
				strcpy(buffer, "0");
			}
			write("\t.long\t%s", buffer);
			break;
		}
		break;
	case SYMBOL_PARAM:
		break;
	case SYMBOL_LOCAL:
		loc_from_symbol(buffer, d->symbol);
		if (d->value) {
			codegen_expr(d->value);
			write("\tmovl\t%s, %s", reg_to_s(d->value->reg), buffer);
		} else {
			write("\tmovl\t$0, %s", buffer);
		}
		break;
	}
	
	codegen_decl(d->next);
}

void codegen_stmt(struct stmt *s)
{
	if (!s) {
		return;
	}
	
	static int label_count = 0;
	int label = ++label_count;
	struct expr *e = s->expr;
	switch (s->kind) {
	case STMT_DECL:
		codegen_decl(s->decl);
		break;
	case STMT_EXPR:
		codegen_expr(e);
		break;
	case STMT_IF_ELSE:
		write(".if%d:", label);
		codegen_expr(e);
		write("\tcmpl\t$0, %s", reg_to_s(e->reg));
		write("\tje\t.else%d", label);
		write(".then%d:", label);
		codegen_stmt(s->body);
		write("\tjmp\t.endif%d", label);
		write(".else%d:", label);
		codegen_stmt(s->ebody);
		write(".endif%d:", label);
		break;
	case STMT_WHILE:
		write(".while%d:", label);
		codegen_expr(e);
		write("\tcmpl\t$0, %s", reg_to_s(e->reg));
		write("\tje\t.endwhile%d", label);
		write(".whilebody%d:", label);
		codegen_stmt(s->body);
		write("\tjmp\t.while%d", label);
		write(".endwhile%d:", label);
		break;
	case STMT_RETURN:
		codegen_expr(e);
		write("\tmovl\t%s, %%eax", reg_to_s(e->reg));
		write("\tjmp\t.%sret", func_name);
		break;
	case STMT_BLOCK:
		codegen_stmt(s->body);
		break;
	case STMT_PRINT:
		while (e) {
			codegen_expr(e->left);
			write("\tpushl\t%s", reg_to_s(e->left->reg));
			write("\tcall\tprint_%s", type_kind_to_s(expr_to_type_kind(e->left)));
			write("\taddl\t$4, %%esp");
			e = e->right;
		}
		write("\tpushl\t$10");
		write("\tcall\tprint_char");
		write("\taddl\t$4, %%esp");
		break;
	}
	
	codegen_stmt(s->next);
}

/* this could maybe be a function... */
#define CODEGEN_CMP(op) do { \
	write(".cmp%d:", label); \
	write("\tcmpl\t%s, %s", reg_to_s(e->right->reg), reg_to_s(e->left->reg)); \
	write("\t" op "\t.true%d", label); \
	write(".false%d:", label); \
	write("\tmovl\t$0, %s", reg_to_s(e->reg)); \
	write("\tjmp\t.endcmp%d", label); \
	write(".true%d:", label); \
	write("\tmovl\t$1, %s", reg_to_s(e->reg)); \
	write(".endcmp%d:", label); } while (0)
#define CODEGEN_DIV(dest) do { \
	write("\tmovl\t%s, %%eax", reg_to_s(e->left->reg)); \
	if (e->right->reg != e->reg) { \
		write("\tmovl\t%s, %s", reg_to_s(e->right->reg), reg_to_s(e->reg)); \
	} \
	write("\tmovl\t$0, %%edx"); \
	write("\tidivl\t%s", reg_to_s(e->reg)); \
	write("\tmovl\t%%" dest ", %s", reg_to_s(e->reg)); } while (0)

void codegen_expr(struct expr *e)
{
	if (!e) {
		return;
	}
	
	codegen_expr(e->left);
	codegen_expr(e->right);
	
	//write("%d", e->kind);
	
	static int label_count = 0;
	int label = ++label_count;
	char location[16];
	int *ip;
	switch (e->kind) {
	case EXPR_LE:
		CODEGEN_CMP("jle");
		break;
	case EXPR_LT:
		CODEGEN_CMP("jl");
		break;
	case EXPR_EQ:
		CODEGEN_CMP("je");
		break;
	case EXPR_NE:
		CODEGEN_CMP("jne");
		break;
	case EXPR_GT:
		CODEGEN_CMP("jg");
		break;
	case EXPR_GE:
		CODEGEN_CMP("jge");
		break;	
	case EXPR_AND:
		write("\tandl\t%s, %s", reg_to_s(e->right->reg), reg_to_s(e->left->reg));
		break;
	case EXPR_OR:
		write("\torl\t%s, %s", reg_to_s(e->right->reg), reg_to_s(e->left->reg));
		break;
	case EXPR_NOT:
		write("\txorl\t$1, %s", reg_to_s(e->right->reg));
		break;
	case EXPR_POS:
		break;
	case EXPR_NEG:
		write("\tnegl\t%s", reg_to_s(e->right->reg));
		break;
	case EXPR_ADD:
		write("\taddl\t%s, %s", reg_to_s(e->right->reg), reg_to_s(e->left->reg));
		break;
	case EXPR_SUB:
		write("\tsubl\t%s, %s", reg_to_s(e->right->reg), reg_to_s(e->left->reg));
		break;
	case EXPR_MUL:
		write("\timull\t%s, %s", reg_to_s(e->right->reg), reg_to_s(e->left->reg));
		break;
	case EXPR_DIV:
		CODEGEN_DIV("eax");
		break;
	case EXPR_MOD:
		CODEGEN_DIV("edx");
		break;
	case EXPR_POW:
		write("\tpushl\t%s", reg_to_s(e->right->reg));
		write("\tpushl\t%s", reg_to_s(e->left->reg));
		write("\tcall\tpower");
		write("\taddl\t$8, %%esp");
		write("\tmovl\t%%eax, %s", reg_to_s(e->reg));
		break;
	case EXPR_PRE_INCR:
		loc_from_symbol(location, e->right->symbol);
		write("\tincl\t%s", location);
		write("\tmovl\t%s, %s", location, reg_to_s(e->right->reg));
		break;
	case EXPR_PRE_DECR:
		loc_from_symbol(location, e->right->symbol);
		write("\tdecl\t%s", location);
		write("\tmovl\t%s, %s", location, reg_to_s(e->right->reg));
		break;
	case EXPR_POST_INCR:
		loc_from_symbol(location, e->left->symbol);
		write("\tincl\t%s", location);
		break;
	case EXPR_POST_DECR:
		loc_from_symbol(location, e->left->symbol);
		write("\tdecl\t%s", location);
		break;
	case EXPR_INT:
	case EXPR_CHAR:
	case EXPR_BOOLEAN:
		write("\tmovl\t$%d, %s", e->constant, reg_to_s(e->reg));
		break;
	case EXPR_STRING:
		ip = hash_table_lookup(strings, e->name);
		write("\tmovl\t$.string%d, %s", *ip, reg_to_s(e->reg));
		break;
	case EXPR_NAME:
		loc_from_symbol(location, e->symbol);
		write("\tmovl\t%s, %s", location, reg_to_s(e->reg));
		break;
	case EXPR_ASSIGN:
		loc_from_symbol(location, e->symbol);
		write("\tmovl\t%s, %s", reg_to_s(e->right->reg), location);
		break;
	case EXPR_CALL:
		write("\tcall\t%s", e->name);
		/* quick hack to get arity */
		int arity = 0;
		struct expr *arg = e->right;
		while (arg) {
			++arity;
			arg = arg->right;
		}
		if (arity > 0) {
			write("\taddl\t$%d, %%esp", arity * 4);
		}
		write("\tmovl\t%%eax, %s", reg_to_s(e->reg));
		break;
	case EXPR_ARG:
		write("\tpushl\t%s", reg_to_s(e->left->reg));
		break;
	}
}

void loc_from_symbol(char *buffer, struct symbol *s)
{
	switch (s->kind) {
	case SYMBOL_GLOBAL:
		strcpy(buffer, s->name);
		break;
	case SYMBOL_PARAM:
		sprintf(buffer, "%d(%%ebp)", (s->offset + 2) * 4);
		break;
	case SYMBOL_LOCAL:
		sprintf(buffer, "%d(%%ebp)", (s->offset + 1) * -4);
		break;
	}
}
