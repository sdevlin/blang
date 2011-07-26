#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "parse.tab.h"
#include "ast.h"

enum mode {
	MODE_ERROR,
	MODE_HELP,
	MODE_SCAN,
	MODE_PARSE,
	MODE_PRINT,
	MODE_RESOLVE,
	MODE_TYPECHECK,
	MODE_CANON,
	MODE_REDUCE,
	MODE_ANNOTATE,
	MODE_INLINE,
	MODE_PRUNE,
	MODE_ALLOC,
	MODE_CODEGEN
};

extern FILE *yyin;
static struct config config;
static enum mode mode;
static enum mode get_mode(const char *);
static int opt_level;
static void init(void);
static void dispatch(void);

#define ERROR() do { \
	printf("incorrect invocation - use \"blang -help\" for usage\n"); \
	exit(1); } while (0)

int main(int argc, char **argv)
{
	int num_arg = 0;
	yyin = stdin;
	config.fout = stdout;
	config.ferr = stderr;
	--argc, ++argv;
	while (argc > 0) {
		if (*argv[0] == '-') {
			char *flag = *argv + 1;
			if (flag[0] == 'O') {
				opt_level = flag[1] - '0';
			} else {
				mode = get_mode(*argv + 1);
			}
		} else {
			switch (++num_arg) {
			case 1:
				if ((yyin = fopen(*argv, "r")) == NULL) {
					fprintf(stderr, "input file '%s' cannot be opened\n", *argv);
					exit(1);
				}
				break;
			case 2:
				if ((config.fout = fopen(*argv, "w")) == NULL) {
					fprintf(stderr, "output file '%s' cannot be opened\n", *argv);
					exit(1);
				}
				break;
			case 3:
				if ((config.ferr = fopen(*argv, "w")) == NULL) {
					fprintf(stderr, "error file '%s' cannot be opened\n", *argv);
					exit(1);
				}
				break;
			default:
				break;
			}
		}
		--argc, ++argv;
	}
	
	if (mode == MODE_ERROR) {
		ERROR();
	}
	
	init();
	dispatch();
	
	return 0;
}

enum mode get_mode(const char *arg)
{
	if (!strcmp(arg, "help")) {
		return MODE_HELP;
	} else if (!strcmp(arg, "scan")) {
		return MODE_SCAN;
	} else if (!strcmp(arg, "parse")) {
		return MODE_PARSE;
	} else if (!strcmp(arg, "print")) {
		return MODE_PRINT;
	} else if (!strcmp(arg, "resolve")) {
		return MODE_RESOLVE;
	} else if (!strcmp(arg, "typecheck")) {
		return MODE_TYPECHECK;
	} else if (!strcmp(arg, "canonicalize")) {
		return MODE_CANON;
	} else if (!strcmp(arg, "reduce")) {
		return MODE_REDUCE;
	} else if (!strcmp(arg, "annotate")) {
		return MODE_ANNOTATE;
	} else if (!strcmp(arg, "inline")) {
		return MODE_INLINE;
	} else if (!strcmp(arg, "prune")) {
		return MODE_PRUNE;
	} else if (!strcmp(arg, "allocate")) {
		return MODE_ALLOC;
	} else if (!strcmp(arg, "generate")) {
		return MODE_CODEGEN;
	} else {
		ERROR();
	}
}

static int num_passes;
static ast_pass passes[16];

static void passes_init(ast_pass pass, ...)
{
	int i = 0;
	va_list argp;
	va_start(argp, pass);
	while (pass) {
		passes[i] = pass;
		pass = va_arg(argp, ast_pass);
		++i;
	}
	num_passes = i;
	va_end(argp);
}

static void help(void)
{
	printf("usage: blang MODE [OPTIONS] [INFILE] [OUTFILE] [ERRFILE]\n"
	       "\n"
	       "modes:\n"
	       " (printed in order - with some exceptions, later modes imply earlier ones)\n"
	       " -scan:         print list of tokens\n"
	       " -parse:        parse grammar, output only on error\n"
	       " -print:        reprint ast to outfile\n"
	       " -resolve:      resolve symbols and print summary\n"
	       " -typecheck:    check types for correctness, output only on error\n"
	       " -canonicalize: modify ast to canonical form and print\n"
	       " -reduce:       reduce simple expressions and print\n"
	       " -annotate:     annotate symbols for read/write usage and print summary\n"
	       " -inline:       inline constant local variables and print\n"
	       " -prune:        remove dead code from ast and print\n"
	       " -allocate:     allocate registers to expressions, output only on error\n"
	       " -generate:     generate assembly code\n"
	       "\n"
	       "options:\n"
	       " -On: cycle through optimization passes (reduce, annotate, inline, prune) n times\n");
	exit(0);
}

static void scan(void);
static void parse(void);
static void passes_iter(void);
static int opt_begin = -1;
static int opt_end = -1;

void init(void)
{
	switch (mode) {
	case MODE_ERROR:
		ERROR();
		break;
	case MODE_HELP:
		help();
		break;
	case MODE_SCAN:
	case MODE_PARSE:
		break;
	case MODE_PRINT:
		passes_init(ast_print, NULL);
		break;
	case MODE_RESOLVE:
		config.flags |= FLAG_PRINT_RESOLVE;
		passes_init(ast_resolve, NULL);
		break;
	case MODE_TYPECHECK:
		passes_init(ast_resolve, ast_typecheck, NULL);
		break;
	case MODE_CANON:
		passes_init(ast_resolve, ast_typecheck, ast_canon, ast_print, NULL);
		break;
	case MODE_REDUCE:
		passes_init(ast_resolve, ast_typecheck, ast_canon, ast_reduce, ast_print,
		            NULL);
		opt_level = opt_level == 0 ? 1 : opt_level;
		opt_begin = 3;
		opt_end = 4;
		break;
	case MODE_ANNOTATE:
		config.flags |= FLAG_PRINT_ANNOTATE;
		passes_init(ast_resolve, ast_typecheck, ast_canon, ast_reduce,
		            ast_annotate, NULL);
		opt_level = opt_level == 0 ? 1 : opt_level;
		opt_begin = 3;
		opt_end = 5;
		break;
	case MODE_INLINE:
		passes_init(ast_resolve, ast_typecheck, ast_canon, ast_reduce,
		            ast_annotate, ast_inline, ast_print, NULL);
		opt_level = opt_level == 0 ? 1 : opt_level;
		opt_begin = 3;
		opt_end = 6;
		break;
	case MODE_PRUNE:
		passes_init(ast_resolve, ast_typecheck, ast_canon, ast_reduce, 
		            ast_annotate, ast_inline, ast_prune, ast_print, NULL);
		opt_level = opt_level == 0 ? 1 : opt_level;
		opt_begin = 3;
		opt_end = 7;
		break;
	case MODE_ALLOC:
		passes_init(ast_resolve, ast_typecheck, ast_canon, ast_reduce,
		            ast_annotate, ast_inline, ast_prune, ast_alloc, NULL);
		opt_begin = 3;
		opt_end = 7;
		break;
	case MODE_CODEGEN:
		passes_init(ast_resolve, ast_typecheck, ast_canon, ast_reduce,
		            ast_annotate, ast_inline, ast_prune, ast_alloc, ast_codegen,
		            NULL);
		opt_begin = 3;
		opt_end = 7;
		break;
	}
}

void dispatch(void)
{
	switch (mode) {
	case MODE_SCAN:
		scan();
		break;
	default:
		parse();
		passes_iter();
		break;
	}
}

extern char *yytext;
extern int yylex(void);
extern int yyparse(void);
extern struct prog *prog;
static char *format_string(char *);
static char *format_char(char *);

void scan(void)
{
	enum yytokentype token;
	while ((token = yylex())) {
		switch (token) {
		case TOKEN_STRING_LITERAL:
			printf("STRING LITERAL %s\n", format_string(yytext));
			break;
		case TOKEN_CHAR_LITERAL:
			printf("CHAR LITERAL %s\n", format_char(yytext));
			break;
		case TOKEN_INT_LITERAL:
			printf("INT LITERAL\n");
			break;
		case TOKEN_ID:
			printf("IDENTIFIER\n");
			break;
		case TOKEN_INT:
			printf("INT\n");
			break;
		case TOKEN_BOOLEAN:
			printf("BOOLEAN\n");
			break;
		case TOKEN_CHAR:
			printf("CHAR\n");
			break;
		case TOKEN_STRING:
			printf("STRING\n");
			break;
		case TOKEN_VOID:
			printf("VOID\n");
			break;
		case TOKEN_VAR:
			printf("VAR\n");
			break;
		case TOKEN_IF:
			printf("IF\n");
			break;
		case TOKEN_ELSE:
			printf("ELSE\n");
			break;
		case TOKEN_PRINT:
			printf("PRINT\n");
			break;
		case TOKEN_RETURN:
			printf("RETURN\n");
			break;
		case TOKEN_WHILE:
			printf("WHILE\n");
			break;
		case TOKEN_SEMI:
			printf("SEMICOLON\n");
			break;
		case TOKEN_COMMA:
			printf("COMMA\n");
			break;
		case TOKEN_LBRACE:
			printf("LEFT BRACE\n");
			break;
		case TOKEN_RBRACE:
			printf("RIGHT BRACE\n");
			break;
		case TOKEN_LPAREN:
			printf("LEFT PAREN\n");
			break;
		case TOKEN_RPAREN:
			printf("RIGHT PAREN\n");
			break;
		case TOKEN_INCR:
			printf("INCR\n");
			break;
		case TOKEN_DECR:
			printf("DECR\n");
			break;
		case TOKEN_POW:
			printf("POW\n");
			break;
		case TOKEN_ADD:
			printf("ADD\n");
			break;
		case TOKEN_SUB:
			printf("SUB\n");
			break;
		case TOKEN_MUL:
			printf("MUL\n");
			break;
		case TOKEN_DIV:
			printf("DIV\n");
			break;
		case TOKEN_MOD:
			printf("MOD\n");
			break;
		case TOKEN_EQ:
			printf("EQ\n");
			break;
		case TOKEN_NE:
			printf("NE\n");
			break;
		case TOKEN_GE:
			printf("GE\n");
			break;
		case TOKEN_LE:
			printf("LE\n");
			break;
		case TOKEN_GT:
			printf("GT\n");
			break;
		case TOKEN_LT:
			printf("LT\n");
			break;
		case TOKEN_AND:
			printf("AND\n");
			break;
		case TOKEN_OR:
			printf("OR\n");
			break;
		case TOKEN_NOT:
			printf("NOT\n");
			break;
		case TOKEN_ASSIGN:
			printf("ASSIGN\n");
			break;
		case TOKEN_TRUE:
			printf("TRUE\n");
			break;
		case TOKEN_FALSE:
			printf("FALSE\n");
			break;
		}
	}
}

void parse(void)
{
	yyparse();
}

void passes_iter(void)
{
	int i = 0;
	// this loop is pretty hairy
	// it would be nicer to build all the opt passes into the passes array
	while (i < num_passes) {
		if (i == opt_begin && opt_level == 0) {
			i = opt_end;
			continue;
		}
		passes[i](prog, &config);
		++i;
		if (i == opt_end) {
			--opt_level;
			i = opt_begin;
		}
	}
}

static char *format(char *s, char t)
{
	int i = 0, j = 1;
	char c, buffer[256];
	while (s[j] != t) {
		c = s[j++];
		if (c == '\\') {
			c = s[j++];
			switch (c) {
			case 'n':
				buffer[i++] = '\n';
				break;
			case '0':
				buffer[i++] = '\0';
				goto end;
			default:
				buffer[i++] = c;
				break;
			}
		} else {
			buffer[i++] = c;
		}
	}
	buffer[i] = '\0';
end:
	strcpy(s, buffer);
	return s;
}

char *format_string(char *s)
{
	return format(s, '\"');
}

char *format_char(char *s)
{
	return format(s, '\'');
}
