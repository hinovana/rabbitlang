#include "rabbit.h"
#include "node.h"
#include "parse.h"
#include "lexer.h"

static enum lexer_status {
	EXPR_BEG,						/* ignore newline, +/- is a sign. */
	EXPR_MID,						/* newline significant, +/- is a sign. */
	EXPR_END,						/* newline significant, +/- is a operator. */
	EXPR_PAREN,						/* almost like EXPR_END, `do' works as `{'. */
	EXPR_ARG,						/* newline significant, +/- is a operator. */
	EXPR_FNAME,						/* ignore newline, no reserved words. */
	EXPR_DOT,						/* right after `.' or `::', no reserved words. */
	EXPR_CLASS,						/* immediate after `class', no here document. */
} lexer_status;


FILE *yyin;
char *yytext;

int parser_line__ = 0;

void
parser_increment_line()
{
	++parser_line__;
}

int
parser_get_line()
{
	return parser_line__;
}

int
is_digit( int c )
{
	switch(c)
	{
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			return 1;
	}
	return 0;
}

int
is_hexdigit( int c )
{
	switch(c)
	{
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'E':
		case 'F':
			return 1;
	}
	return 0;
}

int
is_cIdentifier( int c )
{
	int i;

	switch(c)
	{
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case '_':
			return 1;
	}

	for( i='A'; i <= 'Z'; ++i ) {
		if( i == c ) {
			return 1;
		}
	}
	for( i='a'; i <= 'z'; ++i ) {
		if( i == c ) {
			return 1;
		}
	}

	return 0;
}

struct keyword_list {
	char* word;
	int   type;
};

int is_keyword( char* str )
{
	static struct keyword_list word_list[] = {
		{ "if",     IF },
		{ "then",   THEN },
		{ "elsif",  ELSE_IF },
		{ "else",   ELSE },
		{ "end",    END },
		{ "while",  WHILE },
		{ "do",     DO },
		{ "break",  BREAK },
		{ "return", RETURN },
		{ "class" , CLASS },
		{ "def",    DEFUN },
		{ "yield",  YIELD },
		{ NULL, 0 }
	};
	int i = 0;
	
	while(1) {
		if( word_list[i].word == NULL ) {
			break;
		}
		if( strcmp( word_list[i].word, str ) == 0 ) {
			return word_list[i].type;
		}
		++i;
	}
	
	return 0;
}

char *rabbit_lexer_buffer = 0;
char *rabbit_lexer_ptr    = 0;

void Init_Lexer( FILE *lexer_in )
{
	char    buffer[4096];
	size_t  n = 0;

	assert( lexer_in );
	
	while( fgets( buffer, sizeof(buffer), lexer_in ) ) {
		n = rabbit_lexer_buffer ? strlen(rabbit_lexer_buffer) : 0;
		n += strlen(buffer) + 1;
		rabbit_lexer_buffer = realloc( rabbit_lexer_buffer, n );
		strcat( rabbit_lexer_buffer, buffer );
	}
	
	n = strlen(rabbit_lexer_buffer);
	
	rabbit_lexer_buffer = realloc( rabbit_lexer_buffer, n + 2 );
	{
		rabbit_lexer_buffer[n]   = '\n';
		rabbit_lexer_buffer[n+1] = '\0';
	}
	rabbit_lexer_ptr = rabbit_lexer_buffer;
}

int
nextc()
{
	if( *rabbit_lexer_ptr == '\0' ) {
		return -1;
	}
	return *rabbit_lexer_ptr++;
}

void
pushback( int c )
{
	rabbit_lexer_ptr--;
}

int
yylex( YYSTYPE *lv )
{
	char c;
	int i =0;

retry:
	c = nextc();

	free(yytext);
	yytext = malloc(2);
	yytext[0]= c;

	switch( c )
	{
	// NULL ^D ^Z EOF
	case '\0': case '\004': case '\032': case -1:
		return 0;

	// ƒzƒƒCƒgƒXƒy[ƒX
	case ' ': case '\t': case '\f': case '\r':
	case '\13':
		goto retry;
	
	// ƒRƒƒ“ƒg
	case '#':
		while( (c = nextc()) != '\n' );
		pushback(c);
		goto retry;

	case '\n':
		parser_increment_line();

		switch (lexer_status){
		case EXPR_BEG:
		case EXPR_FNAME:
		case EXPR_DOT:
			goto retry;
		default:
			break;
		}

		lexer_status = EXPR_BEG;
		return LF;
	
	case '\\':
		c = nextc();
		if (c == '\r') {
			c = nextc();
			if (c != '\n') {
				pushback(c);
			}
		}
		if (c == '\n') {
			parser_increment_line();
			goto retry;
		}
		pushback(c);
		return '\\';
	
	case '"':
	{
		int buffer_size =  LEXER_BUFF_MAX + 1;
		char* buffer = malloc( buffer_size );
		char* ptr = buffer;
		
		*buffer = '\0';
		lexer_status = EXPR_END;

		while(1) {
			c = nextc();

			if( c == -2 ) {
				// error
				fprintf( stderr, "error - `\" •¶ŽšƒŠƒeƒ‰ƒ‹‚Ì“r’†‚ÅEOF‚Å‚·‚æ`\n" );
				abort();
			}
			if( ptr - buffer >= buffer_size - 1 ) {
				int len = strlen(buffer);
				buffer_size += len + LEXER_BUFF_MAX;
				buffer = realloc( buffer, buffer_size );
				ptr = buffer + len;
			}
			if( c == '\\' ) {
				switch(c = nextc())
				{
				case 'b':
					c = '\b';
					break;
				case 't':
					c = '\t';
					break;
				case 'n':
					c = '\n';
					break;
				case 'v':
					c = '\v';
					break;
				case 'f':
					c = '\f';
					break;
				case 'r':
					c = '\r';
					break;
				}
			} else if( c == '"' ) {
				break;
			}
			*ptr++ = c;
			*ptr = '\0';
		}
		lv->node = node_stringvalue_new( buffer );
		free( buffer );
		return VALUE_LITERAL;
	}
	
	case '\'':
	{
		int buffer_size =  LEXER_BUFF_MAX + 1;
		char* buffer = malloc( buffer_size );
		char* ptr = buffer;
		i = 0;
		
		lexer_status = EXPR_END;

		*buffer = '\0';

		if( (c = nextc()) != '\'' ) {
			rabbit_raise( "•Ï‚È•¶Žš‚ª‚ ‚è‚Ü‚µ‚½‚æ``%c", c );
		}
		if( (c = nextc()) != '\'' ) {
			rabbit_raise( "•Ï‚È•¶Žš‚ª‚ ‚è‚Ü‚µ‚½‚æ``%c", c );
		}
		while(1) {
			c = nextc();

			if( c == -1 ) {
				// error
				fprintf( stderr, "error - `''' •¶ŽšƒŠƒeƒ‰ƒ‹‚Ì“r’†‚ÅEOF‚Å‚·‚æ`\n" );
				abort();
			}
			if( c == '\n' ) {
				parser_increment_line();
			}
			
			if( ptr - buffer >= buffer_size - 3 ) {
				int len = strlen(buffer);
				buffer_size += len + LEXER_BUFF_MAX;
				buffer = realloc( buffer, buffer_size );
				ptr = buffer + len;
				++i;
			}
			if( c == '\'' ) {
				if( (c = nextc()) != '\'' ) {
					*ptr++ = '\'';
					*ptr++ = c;
					continue;
				}
				if( (c = nextc()) != '\'' ) {
					*ptr++ = '\'';
					*ptr++ = '\'';
					*ptr++ = c;
					continue;
				}
				break;
			}
			*ptr++ = c;
			*ptr = '\0';
		}

		lv->node = node_stringvalue_new( buffer );
		free( buffer );
		return VALUE_LITERAL;
	}

	case '.':
		lexer_status = EXPR_DOT;
		return DOT;

	case '*':
		if( (c = nextc()) == '=' ) {
			lexer_status = EXPR_BEG;
			return MUL_ASSIGN;
		}
		lexer_status = EXPR_BEG;
		pushback(c);
		return MUL;

	case '/':
		if( (c = nextc()) == '=' ) {
			lexer_status = EXPR_BEG;
			return DIV_ASSIGN;
		}
		lexer_status = EXPR_BEG;
		pushback(c);
		return DIV;

	case '%':
		if( (c = nextc()) == '=' ) {
			lexer_status = EXPR_BEG;
			return MOD_ASSIGN;
		}
		lexer_status = EXPR_BEG;
		pushback(c);
		return MOD;

	case '+':
		if( (c = nextc()) == '=' ) {
			lexer_status = EXPR_BEG;
			return ADD_ASSIGN;
		}
		lexer_status = EXPR_BEG;
		pushback(c);
		return ADD;

	case '-':
		if( (c = nextc()) == '=' ) {
			lexer_status = EXPR_BEG;
			return SUB_ASSIGN;
		}
		lexer_status = EXPR_BEG;
		pushback(c);
		return SUB;

	case '<':
		if( (c = nextc()) == '=' ) {
			lexer_status = EXPR_BEG;
			return LE;
		}
		if( c == '<' ) {
			lexer_status = EXPR_BEG;
			return LTLT;
		}
		lexer_status = EXPR_BEG;
		pushback(c);
		return LT;

	case '>':
		if( (c = nextc()) == '=' ) {
			lexer_status = EXPR_BEG;
			return GE;
		}
		if( c == '>' ) {
			lexer_status = EXPR_BEG;
			return GTGT;
		}
		lexer_status = EXPR_BEG;
		pushback(c);
		return GT;

	case '=':
		if( (c = nextc()) == '=' ) {
			lexer_status = EXPR_BEG;
			return EQ;
		}
		lexer_status = EXPR_BEG;
		pushback(c);
		return ASSIGN;

	case '!':
		if( (c = nextc()) == '=' ) {
			lexer_status = EXPR_BEG;
			return NE;
		}
		lexer_status = EXPR_BEG;
		pushback(c);
		return BANG;

	case '[':
		if( lexer_status == EXPR_FNAME  || lexer_status == EXPR_DOT ) {
			if( (c = nextc()) == ']' ) {
				if( (c = nextc()) == '=' ) {
					lexer_status = EXPR_END;
					return LB_RB_ASSIGN;
				}
				pushback(c);
				lexer_status = EXPR_END;
				return LB_RB;
			}
			pushback(c);
		}
		lexer_status = EXPR_BEG;
		return LB;
	
	case '{':
		lexer_status = EXPR_BEG;
		return LC;
	
	case '|':
		lexer_status = EXPR_BEG;
		return VERTICAL_BAR;
	
	case '(':
		lexer_status = EXPR_BEG;
		return tLP;
	
	case '?':
		lexer_status = EXPR_BEG;
		return HATENA;
	
	case ':':
		lexer_status = EXPR_BEG;
		return COLON;

	case ';':
		lexer_status = EXPR_BEG;
		return SEMICOLON;

	case ',':
		lexer_status = EXPR_BEG;
		return COMMA;

	case ']':
		lexer_status = EXPR_END;
		return RB;

	case '}':
		lexer_status = EXPR_END;
		return RC;

	case ')':
		lexer_status = EXPR_END;
		return RP;
	
	case '0': 
	{
		NODE node;
		char  buffer[LEXER_BUFF_MAX];
		char* ptr = buffer;

		lexer_status = EXPR_END;

		if( (c = nextc()) != 'x' ) {
			pushback(c);
			node = node_intvalue_new(0);
			lv->node = node;
			return VALUE_LITERAL;
		}
		
		*ptr++ = '0';
		*ptr++ = 'x';
		
		c = nextc();
		
		if( !is_hexdigit(c) ) {
			fprintf( stderr, "lexer error - `0x%c'\n", c  );
			abort();
		}
		*ptr++ = c;

		while( (c = nextc()) ) {
			if( !is_hexdigit(c) ) {
				pushback(c);
				break;
			}
			*ptr++ = c;
		}
		*ptr = '\0';

		node = node_intvalue_new( strtol( buffer, NULL, 16 ) );
		lv->node = node;

		return VALUE_LITERAL;
	}

	case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	{
		NODE node;
		char  buffer[LEXER_BUFF_MAX];
		char* ptr = buffer;

		lexer_status = EXPR_END;

		*ptr++ = c;

		while(1) {
			c = nextc();
			if( !is_digit(c) ) {
				pushback(c);
				break;
			}
			*ptr++ = c;
		}
		*ptr = '\0';

		node = node_intvalue_new( strtol( buffer, NULL, 10 ) );
		lv->node = node;

		return VALUE_LITERAL;
	}

	} // end switch


	{
		char  buffer[LEXER_BUFF_MAX];
		char* ptr = buffer;
		int is_call_identifier = 0;

		if( c == '@' || c == '$') {
			*ptr++ = c;
			c = nextc();
		}

		if( is_cIdentifier(c) ) {
			int type;
			
			*ptr++ = c;

			while( (c = nextc() ) ) {
				if( !is_cIdentifier(c) ) {
					if( c == '!' || c == '?' ) {
						*ptr++ = c;
						is_call_identifier = 1;
					} else {
						pushback(c);
					}
					break;
				}
				*ptr++ = c;
			}
			*ptr = '\0';
			
			if( ( type = is_keyword( buffer ) ) == 0 ) {
				if( strcmp( buffer, "true" ) == 0 ) {
					lv->node = node_truevalue_new();
					lexer_status = EXPR_END;
					return VALUE_LITERAL;
				}
				if( strcmp( buffer, "false" ) == 0 ) {
					lv->node = node_falsevalue_new();
					lexer_status = EXPR_END;
					return VALUE_LITERAL;
				}
				lv->identifier = strdup( buffer );
				lexer_status = EXPR_END;

				return is_call_identifier ? CALL_IDENTIFIER : IDENTIFIER;
			}
			switch( lexer_status )
			{
			case EXPR_DOT:
				lv->identifier = strdup( buffer );
				lexer_status = EXPR_END;
				return IDENTIFIER;
			default:
				if( type == DEFUN ) {
					lexer_status = EXPR_FNAME;
				} else {
					lexer_status = EXPR_END;
				}
				return type;
			}
		}
	}
	fprintf( stderr, "lexer unknown token `%c'\n", c );
	abort();
}


