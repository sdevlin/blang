%code top {
	#include <stdlib.h>
	#include <string.h>
	#include <stdio.h>
	#define YYERROR_VERBOSE 1	
	#include "ast.h"
}

%union {
	struct type *type;
	struct expr *expr;
	struct symbol *symbol;
	struct param *param;
	struct stmt *stmt;
	struct decl *decl;
	struct prog *prog;
	char *name;
}

%code {
	extern char *yytext;
	extern int yylex(void);
	static int ordinal(char *);
	static int yyerror(char *);	
	struct prog *prog;
}

%token TOKEN_INT
%token TOKEN_BOOLEAN
%token TOKEN_CHAR
%token TOKEN_STRING
%token TOKEN_VOID
%token TOKEN_VAR
%token TOKEN_IF
%token TOKEN_ELSE
%token TOKEN_PRINT
%token TOKEN_RETURN
%token TOKEN_WHILE
%token TOKEN_SEMI
%token TOKEN_COMMA
%token TOKEN_LBRACE
%token TOKEN_RBRACE
%token TOKEN_LPAREN
%token TOKEN_RPAREN
%token TOKEN_INCR
%token TOKEN_DECR
%token TOKEN_POW
%token TOKEN_ADD
%token TOKEN_SUB
%token TOKEN_MUL
%token TOKEN_DIV
%token TOKEN_MOD
%token TOKEN_EQ
%token TOKEN_NE
%token TOKEN_GE
%token TOKEN_LE
%token TOKEN_GT
%token TOKEN_LT
%token TOKEN_AND
%token TOKEN_OR
%token TOKEN_NOT
%token TOKEN_ASSIGN
%token TOKEN_TRUE
%token TOKEN_FALSE
%token TOKEN_STRING_LITERAL
%token TOKEN_CHAR_LITERAL
%token TOKEN_INT_LITERAL
%token TOKEN_ID

%type <type> type
%type <expr> maybe_arg_list arg_list expr and_expr or_expr cmp_expr add_expr mul_expr pow_expr unary_expr incr_expr atomic_expr name_expr constant
%type <name> name
%type <param> maybe_param_list param_list
%type <stmt> block stmt_list stmt bound_stmt common_stmt
%type <decl> global_list global local
%type <prog> program

%%

program
	: global_list { prog = prog_make($1); }
	;

global_list
	: /* maybe not */ { $$ = NULL; }
	| global global_list { $$ = $1; $$->next = $2; }
	;

global
	: type name TOKEN_SEMI { $$ = decl_make($2, $1, NULL, NULL); }
	| type name TOKEN_ASSIGN expr TOKEN_SEMI { $$ = decl_make($2, $1, $4, NULL); }
	| type name TOKEN_LPAREN maybe_param_list TOKEN_RPAREN TOKEN_SEMI
	{ struct type *t = type_make(TYPE_FUNCTION, $4, $1);
	$$ = decl_make($2, t, NULL, NULL); }
	| type name TOKEN_LPAREN maybe_param_list TOKEN_RPAREN block
	{ struct type *t = type_make(TYPE_FUNCTION, $4, $1);
	$$ = decl_make($2, t, NULL, $6); }
	;

local
	: type name TOKEN_SEMI { $$ = decl_make($2, $1, NULL, NULL); }
	| type name TOKEN_ASSIGN expr TOKEN_SEMI { $$ = decl_make($2, $1, $4, NULL); }
	;

maybe_param_list
	: /* maybe not */ { $$ = NULL; }
	| param_list { $$ = $1; }
	;

param_list
	: type name { $$ = param_make($2, $1, NULL); }
	| type name TOKEN_COMMA param_list { $$ = param_make($2, $1, $4); }
	;

maybe_arg_list
	: /* maybe not */ { $$ = NULL; }
	| arg_list { $$ = $1; }
	;

arg_list
	: expr { $$ = expr_make(EXPR_ARG, $1, NULL, NULL, 0); }
	| expr TOKEN_COMMA arg_list { $$ = expr_make(EXPR_ARG, $1, $3, NULL, 0); }
	;

block
	: TOKEN_LBRACE stmt_list TOKEN_RBRACE
	{ $$ = stmt_make(STMT_BLOCK, NULL, NULL, $2, NULL); }
	;

stmt_list
	: /* maybe not */ { $$ = NULL; }
	| stmt stmt_list { $$ = $1; $$->next = $2; }
	;

stmt
	: common_stmt { $$ = $1; }
	| TOKEN_IF TOKEN_LPAREN expr TOKEN_RPAREN stmt
	{ $$ = stmt_make(STMT_IF_ELSE, NULL, $3, $5, NULL); }
	| TOKEN_IF TOKEN_LPAREN expr TOKEN_RPAREN bound_stmt TOKEN_ELSE stmt
	{ $$ = stmt_make(STMT_IF_ELSE, NULL, $3, $5, $7); }
	| TOKEN_WHILE TOKEN_LPAREN expr TOKEN_RPAREN stmt
	{ $$ = stmt_make(STMT_WHILE, NULL, $3, $5, NULL); }
	;

bound_stmt
	: common_stmt { $$ = $1; }
	| TOKEN_IF TOKEN_LPAREN expr TOKEN_RPAREN bound_stmt TOKEN_ELSE bound_stmt
	{ $$ = stmt_make(STMT_IF_ELSE, NULL, $3, $5, $7); }
	| TOKEN_WHILE TOKEN_LPAREN expr TOKEN_RPAREN bound_stmt
	{ $$ = stmt_make(STMT_WHILE, NULL, $3, $5, NULL); }
	;

common_stmt
	: local { $$ = stmt_make(STMT_DECL, $1, NULL, NULL, NULL); }
	| expr TOKEN_SEMI { $$ = stmt_make(STMT_EXPR, NULL, $1, NULL, NULL); }
	| TOKEN_RETURN expr TOKEN_SEMI
	{ $$ = stmt_make(STMT_RETURN, NULL, $2, NULL, NULL); }
	| block { $$ = $1; }
	| TOKEN_PRINT arg_list TOKEN_SEMI
	{ $$ = stmt_make(STMT_PRINT, NULL, $2, NULL, NULL); }
	;

expr
	: name TOKEN_ASSIGN expr { $$ = expr_make(EXPR_ASSIGN, NULL, $3, $1, 0); }
	| or_expr { $$ = $1; }
	;

or_expr
	: or_expr TOKEN_OR and_expr { $$ = expr_make(EXPR_OR, $1, $3, NULL, 0); }
	| and_expr { $$ = $1; }
	;

and_expr
	: and_expr TOKEN_AND cmp_expr { $$ = expr_make(EXPR_AND, $1, $3, NULL, 0); }
	| cmp_expr { $$ = $1; }
	;

cmp_expr
	: cmp_expr TOKEN_LT add_expr { $$ = expr_make(EXPR_LT, $1, $3, NULL, 0); }
	| cmp_expr TOKEN_LE add_expr { $$ = expr_make(EXPR_LE, $1, $3, NULL, 0); }
	| cmp_expr TOKEN_GE add_expr { $$ = expr_make(EXPR_GE, $1, $3, NULL, 0); }
	| cmp_expr TOKEN_GT add_expr { $$ = expr_make(EXPR_GT, $1, $3, NULL, 0); }
	| cmp_expr TOKEN_EQ add_expr { $$ = expr_make(EXPR_EQ, $1, $3, NULL, 0); }
	| cmp_expr TOKEN_NE add_expr { $$ = expr_make(EXPR_NE, $1, $3, NULL, 0); }
	| add_expr { $$ = $1; }
	;

add_expr
	: add_expr TOKEN_ADD mul_expr { $$ = expr_make(EXPR_ADD, $1, $3, NULL, 0); }
	| add_expr TOKEN_SUB mul_expr { $$ = expr_make(EXPR_SUB, $1, $3, NULL, 0); }
	| mul_expr { $$ = $1; }
	;

mul_expr
	: mul_expr TOKEN_MUL pow_expr { $$ = expr_make(EXPR_MUL, $1, $3, NULL, 0); }
	| mul_expr TOKEN_DIV pow_expr { $$ = expr_make(EXPR_DIV, $1, $3, NULL, 0); }
	| mul_expr TOKEN_MOD pow_expr { $$ = expr_make(EXPR_MOD, $1, $3, NULL, 0); }
	| pow_expr { $$ = $1; }
	;

pow_expr
	: unary_expr TOKEN_POW pow_expr { $$ = expr_make(EXPR_POW, $1, $3, NULL, 0); }
	| unary_expr { $$ = $1; }
	;

unary_expr
	: TOKEN_ADD unary_expr { $$ = expr_make(EXPR_POS, NULL, $2, NULL, 0); }
	| TOKEN_SUB unary_expr { $$ = expr_make(EXPR_NEG, NULL, $2, NULL, 0); }
	| TOKEN_NOT unary_expr { $$ = expr_make(EXPR_NOT, NULL, $2, NULL, 0); }
	| incr_expr { $$ = $1; }
	;

incr_expr
	: TOKEN_INCR name_expr { $$ = expr_make(EXPR_PRE_INCR, NULL, $2, NULL, 0); }
	| TOKEN_DECR name_expr { $$ = expr_make(EXPR_PRE_DECR, NULL, $2, NULL, 0); }
	| name_expr TOKEN_INCR { $$ = expr_make(EXPR_POST_INCR, $1, NULL, NULL, 0); }
	| name_expr TOKEN_DECR { $$ = expr_make(EXPR_POST_DECR, $1, NULL, NULL, 0); }
	| atomic_expr { $$ = $1; }
	;

atomic_expr
	: name_expr { $$ = $1; }
	| constant { $$ = $1; }
	| name TOKEN_LPAREN maybe_arg_list TOKEN_RPAREN
	{ $$ = expr_make(EXPR_CALL, NULL, $3, $1, 0); }
	| TOKEN_LPAREN expr TOKEN_RPAREN { $$ = $2; }
	;

name_expr
	: name { $$ = expr_make(EXPR_NAME, NULL, NULL, $1, 0); }
	;

name
	: TOKEN_ID { $$ = strdup(yytext); }
	;

constant
	: TOKEN_INT_LITERAL
	{ $$ = expr_make(EXPR_INT, NULL, NULL, NULL, strtol(yytext, NULL, 10)); }
	| TOKEN_TRUE { $$ = expr_make(EXPR_BOOLEAN, NULL, NULL, NULL, 1); }
	| TOKEN_FALSE { $$ = expr_make(EXPR_BOOLEAN, NULL, NULL, NULL, 0); }
	| TOKEN_CHAR_LITERAL
	{ $$ = expr_make(EXPR_CHAR, NULL, NULL, strdup(yytext), ordinal(yytext)); }
	| TOKEN_STRING_LITERAL
	{ $$ = expr_make(EXPR_STRING, NULL, NULL, strdup(yytext), 0); }
	;

type
	: TOKEN_INT { $$ = type_make(TYPE_INT, NULL, NULL); }
	| TOKEN_BOOLEAN { $$ = type_make(TYPE_BOOLEAN, NULL, NULL); }
	| TOKEN_CHAR { $$ = type_make(TYPE_CHAR, NULL, NULL); }
	| TOKEN_STRING { $$ = type_make(TYPE_STRING, NULL, NULL); }
	| TOKEN_VOID { $$ = type_make(TYPE_VOID, NULL, NULL); }
	| TOKEN_VAR { $$ = type_make(TYPE_UNKNOWN, NULL, NULL); }
	;

%%

static int ordinal(char *s)
{
	++s;
	if (*s == '\\') {
		++s;
		if (*s == 'n') {
			return '\n';
		} else if (*s == 't') {
			return '\t';
		}
	}
	return *s;
}

static int yyerror(char *s)
{
	fprintf(stderr, "parse: %s\n", s);
	exit(1);
}
