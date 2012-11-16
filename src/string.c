#include "rabbit.h"


VALUE
rabbit_string_new( size_t len, char* ptr )
{
	struct RString* str;

	RABBIT_OBJECT_NEW( str, struct RString );
	RABBIT_OBJECT_SETUP( str, C_String, TYPE_STRING );
	{
		str->len = len;
		str->ptr = 0;
		
		if( ptr ) {
			str->ptr = rabbit_malloc( len + 1 );
			strncpy( str->ptr, ptr, len );
			str->ptr[len] = '\0';
		}
	}
	return  (VALUE)str;
}

VALUE
rabbit_string_s_new( VALUE self, int argc, VALUE *argv )
{
	struct RString *argval, *str;
	
	if( argc > 0 ) {
		if( VALUE_TYPE(argv[0]) != TYPE_STRING ) {
			rabbit_raise2( "error rabbit_string_s_new - 引数がおかしいです。");
		}
		argval = R_STRING(argv[0]);
		str = (struct RString*)rabbit_string_new( argval->len, argval->ptr );
	} else {
		str = (struct RString*)rabbit_string_new(0,0);
	}

	if( self == R_NULL ) {
		if( VALUE_TYPE(self) != TYPE_CLASS ) {
			rabbit_raise( "selfの型がなんかおかしいです。(%d)", VALUE_TYPE(self) );
		}
		R_BASIC(str)->klass = self;
	}
	return (VALUE)str;
}

VALUE
rabbit_string_length( VALUE self )
{
	assert( self );
	
	if( VALUE_TYPE(self) != TYPE_STRING ) {
		fprintf( stderr, "error rabbit_string_length - selfの型がおかしいです。\n" );
		abort();
	}
	
	return INT2FIX(R_STRING(self)->len);
}

VALUE
rabbit_string_add( VALUE self, VALUE right )
{
	VALUE result;
	char *str;
	size_t len;

	assert( self );

	if( VALUE_TYPE(self) != TYPE_STRING ) {
		fprintf( stderr, "error rabbit_string_length - selfの型がおかしいです。\n" );
		abort();
	}
	if( VALUE_TYPE(right) != TYPE_STRING ) {
		fprintf( stderr, "type error rabbit_string_add - 右辺の型がおかしいです。\n" );
		abort();
	}
	
	len = R_STRING(self)->len + R_STRING(right)->len;
	str = rabbit_calloc( sizeof(char), len + 1 );

	strncpy( str, R_STRING(self)->ptr, R_STRING(self)->len );
	strncat( str, R_STRING(right)->ptr, R_STRING(right)->len );

	result = rabbit_string_new( len, str );
	free(str);

	return result;
}

VALUE
rabbit_string_each_byte( VALUE self )
{
	int i;
	VALUE argv[1];

	assert( self );

	if( VALUE_TYPE(self) != TYPE_STRING ) {
		fprintf( stderr, "error rabbit_string_length - selfの型がおかしいです。\n" );
		abort();
	}

	for( i=0; i<R_STRING(self)->len; ++i ) {
		char c[2] = { R_STRING(self)->ptr[i] };
		c[1] = '\0';
		argv[0] = rabbit_string_new( 1, c );

		rabbit_yield( self, 1, argv );
	}
	return R_NULL;
}

VALUE
rabbit_string_to_integer( VALUE self )
{
	VALUE result;
	
	assert( self );

	if( VALUE_TYPE(self) != TYPE_STRING ) {
		fprintf( stderr, "error rabbit_string_length - selfの型がおかしいです。\n" );
		abort();
	}
	result = INT2FIX( strtol( R_STRING(self)->ptr, NULL, 10 ) );

	return result;
}

VALUE eval_string__( char* str )
{
	extern VALUE rabbit_last_value;
	extern char *rabbit_lexer_buffer;
	extern char *rabbit_lexer_ptr;
	extern int yyparse(void);
	size_t n = 0;

	assert( str );

	n = strlen( str );

	rabbit_lexer_buffer = malloc( n + 2 );
	{
		strncpy( rabbit_lexer_buffer, str, n );
		rabbit_lexer_buffer[ n ]     = '\n';
		rabbit_lexer_buffer[ n + 1 ] = '\0';
	}
	rabbit_lexer_ptr = rabbit_lexer_buffer;

	if( yyparse() ) {
		fprintf( stderr, "[(;_;)]Rabbit parser error\n");
	}
	free( rabbit_lexer_buffer );
	rabbit_lexer_ptr = 0;
	
	return rabbit_last_value;
}

VALUE rabbit_string_eval( VALUE self )
{
	TypeCheck( self, TYPE_STRING );
	
	if( R_STRING(self)->len ) {
		return eval_string__( R_STRING(self)->ptr );
	}
	return R_NULL;
}

VALUE rabbit_f_eval( VALUE self, VALUE str )
{
	TypeCheck( str, TYPE_STRING );
	
	if( R_STRING(str)->len ) {
		return eval_string__( R_STRING(str)->ptr );
	}
	return R_NULL;
}

VALUE rabbit_string_eq( VALUE self, VALUE s )
{
	TypeCheck( self, TYPE_STRING );
	
	if( VALUE_TYPE(s) != TYPE_STRING ) {
		return R_FALSE;
	}
	if( R_STRING(s)->len == 0 || R_STRING(s)->len != R_STRING(self)->len ) {
		return R_FALSE;
	}
	if( strncmp( R_STRING(self)->ptr, R_STRING(s)->ptr, R_STRING(self)->len ) != 0 ) {
		return R_FALSE;
	}
	return R_TRUE;
}

VALUE rabbit_string_ne( VALUE self, VALUE s )
{
	return rabbit_string_eq( self, s ) == R_TRUE ? R_FALSE : R_TRUE;
}


void
String_Setup()
{
	C_String = rabbit_define_class( "String", C_Object );

	rabbit_define_singleton_method( C_String, "new", rabbit_string_s_new, -1 );

	rabbit_define_method( C_String, "len", rabbit_string_length, 0 );
	rabbit_define_method( C_String, "+", rabbit_string_add, 1 );
	rabbit_define_method( C_String, "==", rabbit_string_eq, 1 );
	rabbit_define_method( C_String, "eql?", rabbit_string_eq, 1 );
	rabbit_define_method( C_String, "!=", rabbit_string_ne, 1 );

	rabbit_define_method( C_String, "each_byte", rabbit_string_each_byte, 0 );
	rabbit_define_method( C_String, "to_i", rabbit_string_to_integer, 0 );
	rabbit_define_method( C_String, "eval", rabbit_string_eval, 0 );
	
}

