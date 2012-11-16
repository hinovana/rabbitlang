#ifndef RABBIT_LEXER_H_
#define RABBIT_LEXER_H_

#define LEXER_BUFF_MAX 4096

int yyerror( char const *str );
int yylex( YYSTYPE *lv );
void parser_increment_line();

#endif // RABBIT_LEXER_H_

