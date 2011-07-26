%option noyywrap
%option noinput
%option nounput
%top {
	#include "parse.tab.h"
	static void validate_chars(void);
	static int length(void);
	static void format(char);
}
DIGIT	[[:digit:]]
ID	[[:alpha:]_][[:alnum:]_]{0,255}
%%
\/\/.*\n?	|
\/\*([^*]|[\r\n]|(\*+([^*/]|[\r\n])))*\*+\/	|
[ \t\r\n]*	;
int	return TOKEN_INT;
boolean	return TOKEN_BOOLEAN;
char	return TOKEN_CHAR;
string	return TOKEN_STRING;
void	return TOKEN_VOID;
var	return TOKEN_VAR;
if	return TOKEN_IF;
else	return TOKEN_ELSE;
print	return TOKEN_PRINT;
return	return TOKEN_RETURN;
while	return TOKEN_WHILE;
;	return TOKEN_SEMI;
,	return TOKEN_COMMA;
"{"	return TOKEN_LBRACE;
"}"	return TOKEN_RBRACE;
"("	return TOKEN_LPAREN;
")"	return TOKEN_RPAREN;
"++"	return TOKEN_INCR;
"--"	return TOKEN_DECR;
"^"	return TOKEN_POW;
"+"	return TOKEN_ADD;
"-"	return TOKEN_SUB;
"*"	return TOKEN_MUL;
"/"	return TOKEN_DIV;
"%"	return TOKEN_MOD;
"=="	return TOKEN_EQ;
"!="	return TOKEN_NE;
">="	return TOKEN_GE;
"<="	return TOKEN_LE;
">"	return TOKEN_GT;
"<"	return TOKEN_LT;
"&&"	return TOKEN_AND;
"||"	return TOKEN_OR;
"!"	return TOKEN_NOT;
"="	return TOKEN_ASSIGN;
true	return TOKEN_TRUE;
false	return TOKEN_FALSE;
\"(\\.|[^\"])*\"	{
	validate_chars();
	if (length() > 256) {
		fprintf(stderr, "scan: string exceeds max length: %s\n", yytext);
		exit(1);
	}
	format('\"');
	return TOKEN_STRING_LITERAL; }
\'[^\'\\]\'	|
\'\\.\'	{ 
	validate_chars();
	format('\''); 
	return TOKEN_CHAR_LITERAL; }
{DIGIT}+	{
	long value;
	errno = 0;
	value = strtol(yytext, NULL, 10);
	if (errno == ERANGE || value > 2147483647) {
		fprintf(stderr, "scan: int exceeds size limits: %s\n", yytext);
		exit(1);
	}
	return TOKEN_INT_LITERAL; }
{ID}	return TOKEN_ID;
.	{
	fprintf(stderr, "scan: unrecognized token: %s\n", yytext);
	exit(1); }
%%
void validate_chars(void)
{
	int i;
	for (i = 0; i < yyleng; ++i) {
		int c = yytext[i];
		if ((c < 0x20) || (c > 0x7e)) {
			fprintf(stderr, "scan: invalid char in string or char literal: %d\n", c);
			exit(1);
		}
	}
}

int length(void)
{
	int len = 0;
	int i = 1;
	while (yytext[i] != '"') {
		if (yytext[i] == '\\') {
			++i;
		}
		++i;
		++len;
	}
	return len;
}

void format(char t)
{
	int i = 1, j = 1;
	char c;
	while (yytext[j] != t) {
		c = yytext[j++];
		if (c == '\\') {
			c = yytext[j++];
			switch (c) {
			case 'n':
				yytext[i++] = '\\';
				yytext[i++] = 'n';
				break;
			case '\\':
				yytext[i++] = '\\';
				yytext[i++] = '\\';
				break;
			case '0':
				yytext[i++] = '\\';
				yytext[i++] = '0';
				break;
			default:
				yytext[i++] = c;
				break;
			}
		} else {
			yytext[i++] = c;
		}
	}
	yytext[i++] = t;
	yytext[i] = '\0';
}
