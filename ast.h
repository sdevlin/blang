#ifndef AST_INCLUDED
#define AST_INCLUDED
#include <stdio.h>
#include "hash_table.h"

struct prog {
	struct decl *ast;
	struct hash_table *strings;
	struct symbol *symbols;
};

extern struct prog *prog_make(struct decl *ast);
extern void prog_add_string(struct prog *prog, const char *string);
extern void prog_free(struct prog **pp);

enum reg {
	REG_EBX = 1,
	REG_ECX = 2,
	REG_EDX = 4,
	REG_ESI = 8,
	REG_EDI = 16,
	REG_EAX = 32
};

extern const char *reg_to_s(enum reg);

struct decl {
	char *name;
	struct type *type;
	struct expr *value;
	struct stmt *code;
	struct symbol *symbol;
	struct decl *next;
	int num_locals;
	enum reg regs;
};

extern struct decl *decl_make(char *name, struct type *type, struct expr *value, struct stmt *code);
extern void decl_free(struct decl **dp);

enum type_kind {
	TYPE_UNKNOWN,
	TYPE_CHAR,
	TYPE_INT,
	TYPE_STRING,
	TYPE_BOOLEAN,
	TYPE_VOID,
	TYPE_FUNCTION
};

extern const char *type_kind_to_s(enum type_kind kind);

struct type {
	enum type_kind kind;
	struct param *params;
	struct type *rtype;
};

extern struct type *type_make(enum type_kind kind, struct param *params, struct type *rtype);
extern void type_free(struct type **tp);

enum expr_kind {
	EXPR_LE = 1,
	EXPR_LT,
	EXPR_EQ,
	EXPR_NE,
	EXPR_GT,
	EXPR_GE,
	EXPR_NOT,
	EXPR_ADD,
	EXPR_SUB,
	EXPR_MUL,
	EXPR_DIV,
	EXPR_POS,
	EXPR_NEG,
	EXPR_MOD,
	EXPR_ASSIGN,
	EXPR_CALL,
	EXPR_ARG,
	EXPR_NAME,
	EXPR_INT,
	EXPR_CHAR,
	EXPR_BOOLEAN,
	EXPR_PRE_INCR,
	EXPR_PRE_DECR,
	EXPR_POST_INCR,
	EXPR_POST_DECR,
	EXPR_OR,
	EXPR_AND,
	EXPR_POW,
	EXPR_STRING
};

extern const char *expr_kind_to_s(enum expr_kind kind);

struct expr {
	enum expr_kind kind;
	struct expr *left;
	struct expr *right;
	char *name;
	int constant;
	struct symbol *symbol;
	enum reg reg;
};

extern struct expr *expr_make(enum expr_kind kind, struct expr *left, struct expr *right, char *name, int constant);
extern struct expr *expr_copy(struct expr *e);
extern void expr_free(struct expr **ep);
extern enum type_kind expr_to_type_kind(struct expr *e);
extern int expr_is_const(struct expr *e);
extern int expr_has_effects(struct expr *e);

enum stmt_kind {
	STMT_DECL,
	STMT_EXPR,
	STMT_IF_ELSE,
	STMT_WHILE,
	STMT_RETURN,
	STMT_BLOCK,
	STMT_PRINT
};

struct stmt {
	enum stmt_kind kind;
	struct decl *decl;
	struct expr *expr;
	struct stmt *body;
	struct stmt *ebody;
	struct stmt *next;
};

extern struct stmt *stmt_make(enum stmt_kind kind, struct decl *decl, struct expr *expr, struct stmt *body, struct stmt *ebody);
extern void stmt_free(struct stmt **sp);

enum symbol_kind {
	SYMBOL_GLOBAL,
	SYMBOL_PARAM,
	SYMBOL_LOCAL
};

struct symbol {
	enum symbol_kind kind;
	int which;
	struct type *type;
	char *name;
	int offset;
	int init;
	struct expr *value;
	int num_reads;
	int num_writes;
	struct symbol *next;
};

extern struct symbol *symbol_make(enum symbol_kind kind, struct type *type, char *name, struct prog *prog);
extern void symbol_free(struct symbol **sp);

struct param {
	char *name;
	struct type *type;
	struct param *next;
};

extern struct param *param_make(char *name, struct type *type, struct param *next);
extern void param_free(struct param **pp);

enum config_flag {
	FLAG_PRINT_RESOLVE = 1,
	FLAG_PRINT_ANNOTATE = 2
};

struct config {
	FILE *fout;
	FILE *ferr;
	int opt_level;
	enum config_flag flags;
};

typedef void (*ast_pass)(struct prog *, struct config *);
extern void ast_print(struct prog *prog, struct config *cfg);
extern void ast_resolve(struct prog *prog, struct config *cfg);
extern void ast_typecheck(struct prog *prog, struct config *cfg);
extern void ast_canon(struct prog *prog, struct config *cfg);
extern void ast_reduce(struct prog *prog, struct config *cfg);
extern void ast_annotate(struct prog *prog, struct config *cfg);
extern void ast_inline(struct prog *prog, struct config *cfg);
extern void ast_prune(struct prog *prog, struct config *cfg);
extern void ast_alloc(struct prog *prog, struct config *cfg);
extern void ast_codegen(struct prog *prog, struct config *cfg);
#endif
