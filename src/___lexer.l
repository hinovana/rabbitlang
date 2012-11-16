%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rabbit.h"
#include "node.h"
#include "parse.h"

void parser_increment_line();

int yywrap(void) {
	return 1;
}
#define YY_DECL int yylex(YYSTYPE *lv)

/* 警告を避ける */
#define ECHO /* do nothing */
#define YY_NO_INPUT


%}

%option yylineno

%x COMMENT
%x QQ_STRING
%x QQQ_STRING

%%
<INITIAL>\\\n			;
<INITIAL>"$"			return DOLLAR;
<INITIAL>"+"			return ADD;
<INITIAL>"-"			return SUB;
<INITIAL>"*"			return MUL;
<INITIAL>"/"			return DIV;
<INITIAL>"%"			return MOD;
<INITIAL>"("			return tLP;
<INITIAL>")"			return RP;
<INITIAL>"{"			return LC;
<INITIAL>"}"			return RC;
<INITIAL>"["			return LB;
<INITIAL>"]"			return RB;
<INITIAL>"|"			return VERTICAL_BAR;
<INITIAL>"="			return ASSIGN;
<INITIAL>"+="			return ADD_ASSIGN;
<INITIAL>"-="			return SUB_ASSIGN;
<INITIAL>"*="			return MUL_ASSIGN;
<INITIAL>"/="			return DIV_ASSIGN;
<INITIAL>"%="			return MOD_ASSIGN;
<INITIAL>"=="			return EQ;
<INITIAL>"!="			return NE;
<INITIAL>">"			return GT;
<INITIAL>"<"			return LT;
<INITIAL>">="			return GE;
<INITIAL>"<="			return LE;
<INITIAL>","			return COMMA;
<INITIAL>"."			return DOT;
<INITIAL>";"			return SEMICOLON;
<INITIAL>":"			return COLON;
<INITIAL>"!"			return BANG;
<INITIAL>"?"			return HATENA;
<INITIAL>"#"			BEGIN COMMENT;
<INITIAL>"[]"			return LB_RB;
<INITIAL>"[]="			return LB_RB_ASSIGN;
<INITIAL>"if"			return IF;
<INITIAL>"then"			return THEN;
<INITIAL>"else"			return ELSE;
<INITIAL>"end"			return END;
<INITIAL>"do"			return DO;
<INITIAL>"break"		return BREAK;
<INITIAL>"return"		return RETURN;
<INITIAL>"while"		return WHILE;
<INITIAL>"class"		return CLASS;
<INITIAL>"def"			return DEFUN;
<INITIAL>"yield"		return YIELD;
<INITIAL>"func"			return FUNC;
<INITIAL>"true"				{
	lv->node = node_truevalue_new();
	return VALUE_LITERAL;
}
<INITIAL>"false"			{
	lv->node = node_falsevalue_new();
	return VALUE_LITERAL;
}
<INITIAL>[ \t\r]			;
<INITIAL>"\n"				{
	parser_increment_line();
	return LF;
}
<INITIAL>\@[a-zA-Z_][a-zA-Z0-9_]* {
	lv->identifier = strdup( yytext );
	return SELF_IDENTIFIER;
}
<INITIAL>\$[a-zA-Z_][a-zA-Z0-9_]* {
	lv->identifier = strdup( yytext );
	return GLOBAL_IDENTIFIER;
}
<INITIAL>[a-zA-Z_][a-zA-Z0-9_]* {
	lv->identifier = strdup( yytext );
	return IDENTIFIER;
}
<INITIAL>[a-zA-Z_][a-zA-Z0-9_]*[\?\!]{1} {
	lv->identifier = strdup( yytext );
	return CALL_IDENTIFIER;
}
<INITIAL>0|[1-9][0-9]*			{
	NODE node;
	node = node_intvalue_new( strtol( yytext, NULL, 10 ) );
	lv->node = node;
	return VALUE_LITERAL;
}
<INITIAL>0x[0-9a-fA-F]+		{
	NODE node;
	node = node_intvalue_new( strtol( yytext, NULL, 16 ) );
	lv->node = node;
	return VALUE_LITERAL;
}
<INITIAL>\"					{
	lv->strval = (char*)malloc(1);
	lv->strval[0] = '\0';
	BEGIN QQ_STRING;
}
<INITIAL>\'\'\'				{
	lv->strval = (char*)malloc(1);
	lv->strval[0] = '\0';
	BEGIN QQQ_STRING;
}
<INITIAL>. {
	fprintf( stderr, "lexer error '%s'", yytext );
	exit(0);
}
<QQ_STRING>[^\\\"\n]+		{
	int len         = strlen(lv->strval);
	int buffer_size = len + yyleng;

	lv->strval = (char*)realloc( lv->strval, buffer_size + 1 );
	strncat( lv->strval, yytext, yyleng );
}
<QQ_STRING>\\(.|\n)			{
	size_t len = strlen(lv->strval);
	lv->strval = (char *)realloc(lv->strval, len + 1 + 1);
	switch (yytext[yyleng - 1])
	{
	case 'b':
		lv->strval[len] = '\b';  
		break;
	case 't':
		lv->strval[len] = '\t';
		break;
	case 'n':
		lv->strval[len] = '\n';
		break;
	case 'v':
		lv->strval[len] = '\v';
		break;
	case 'f':
		lv->strval[len] = '\f';
		break;
	case 'r':
		lv->strval[len] = '\r';
		break;
	default:
		lv->strval[len] = yytext[yyleng - 1];
	}
	lv->strval[len + 1] = '\0';
}
<QQ_STRING>\n				{
	extern int yylineno;
	fprintf( stderr, "lexer error - line %d  near \\n\n", yylineno );
	exit(1);
}
<QQ_STRING>\"				{
	lv->node = node_stringvalue_new( lv->strval );
	BEGIN INITIAL;
	return VALUE_LITERAL;
}
<QQ_STRING><<EOF>> {
	BEGIN(INITIAL);
	return VALUE_LITERAL;
}


<QQQ_STRING>[^\n\'\'\']+				{
	int len         = strlen(lv->strval);
	int buffer_size = len + yyleng;
	lv->strval = (char*)realloc( lv->strval, buffer_size + 1 );
	strncat( lv->strval, yytext, yyleng );
}
<QQQ_STRING>\\(.|\n)			{
	size_t len = strlen(lv->strval);
	lv->strval = (char *)realloc(lv->strval, len + 1 + 1);
	switch (yytext[yyleng - 1])
	{
	case 'b':
		lv->strval[len] = '\b';  
		break;
	case 't':
		lv->strval[len] = '\t';
		break;
	case 'n':
		lv->strval[len] = '\n';
		break;
	case 'v':
		lv->strval[len] = '\v';
		break;
	case 'f':
		lv->strval[len] = '\f';
		break;
	case 'r':
		lv->strval[len] = '\r';
		break;
	default:
		lv->strval[len] = yytext[yyleng - 1];
	}
	lv->strval[len + 1] = '\0';
}
<QQQ_STRING>\n				{
	int len         = strlen(lv->strval);
	int buffer_size = len + yyleng;
	lv->strval = (char*)realloc( lv->strval, buffer_size + 1 );
	strncat( lv->strval, yytext, yyleng );
}
<QQQ_STRING>\'\'\'			{
	lv->node = node_stringvalue_new( lv->strval );
	BEGIN INITIAL;
	return VALUE_LITERAL;
}
<QQQ_STRING><<EOF>> {
	BEGIN(INITIAL);
	return VALUE_LITERAL;
}

<COMMENT>.					;
<COMMENT>"\n"				{
	parser_increment_line();
	BEGIN INITIAL;
	return LF;
}
%%

