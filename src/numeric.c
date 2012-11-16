#include <time.h>
#include <unistd.h>
#include "rabbit.h"

const char rabbit_digitmap[] = "0123456789abcdefghijklmnopqrstuvwxyz";


// ==
VALUE
rabbit_fixnum_eq( VALUE self, VALUE n )
{
	TypeCheck( self, TYPE_FIXNUM );
	
	if( VALUE_TYPE(n) != TYPE_FIXNUM ) {
		return R_FALSE;
	}
	return FIX2INT(self) == FIX2INT(n) ? R_TRUE : R_FALSE;
}

// !=
VALUE
rabbit_fixnum_ne( VALUE self, VALUE n )
{
	TypeCheck( self, TYPE_FIXNUM );
	
	if( VALUE_TYPE(n) != TYPE_FIXNUM ) {
		return R_FALSE;
	}
	return FIX2INT(self) != FIX2INT(n) ? R_TRUE : R_FALSE;
}

// >
VALUE
rabbit_fixnum_gt( VALUE self, VALUE n )
{
	TypeCheck( self, TYPE_FIXNUM );
	
	if( VALUE_TYPE(n) != TYPE_FIXNUM ) {
		OpErr( ">", self, n );
	}
	return FIX2INT(self) > FIX2INT(n) ? R_TRUE : R_FALSE;
}

// >=
VALUE
rabbit_fixnum_ge( VALUE self, VALUE n )
{
	TypeCheck( self, TYPE_FIXNUM );
	
	if( VALUE_TYPE(n) != TYPE_FIXNUM ) {
		OpErr( ">=", self, n );
	}
	return FIX2INT(self) >= FIX2INT(n) ? R_TRUE : R_FALSE;
}

// <
VALUE
rabbit_fixnum_lt( VALUE self, VALUE n )
{
	TypeCheck( self, TYPE_FIXNUM );
	
	if( VALUE_TYPE(n) != TYPE_FIXNUM ) {
		OpErr( "<", self, n );
	}
	return FIX2INT(self) < FIX2INT(n) ? R_TRUE : R_FALSE;
}

// <=
VALUE
rabbit_fixnum_le( VALUE self, VALUE n )
{
	TypeCheck( self, TYPE_FIXNUM );
	
	if( VALUE_TYPE(n) != TYPE_FIXNUM ) {
		OpErr( "<=", self, n );
	}
	return FIX2INT(self) <= FIX2INT(n) ? R_TRUE : R_FALSE;
}

// +
VALUE
rabbit_fixnum_add( VALUE self, VALUE n )
{
	TypeCheck( self, TYPE_FIXNUM );
	
	if( VALUE_TYPE(n) != TYPE_FIXNUM ) {
		OpErr( "+", self, n );
	}
	return INT2FIX( FIX2INT(self) + FIX2INT(n) );
}

// -
VALUE
rabbit_fixnum_sub( VALUE self, VALUE n )
{
	TypeCheck( self, TYPE_FIXNUM );
	
	if( VALUE_TYPE(n) != TYPE_FIXNUM ) {
		OpErr( "-", self, n );
	}
	return INT2FIX( FIX2INT(self) - FIX2INT(n) );
}

// *
VALUE
rabbit_fixnum_mul( VALUE self, VALUE n )
{
	TypeCheck( self, TYPE_FIXNUM );
	
	if( VALUE_TYPE(n) != TYPE_FIXNUM ) {
		OpErr( "*", self, n );
	}
	return INT2FIX( FIX2INT(self) * FIX2INT(n) );
}

// /
VALUE
rabbit_fixnum_div( VALUE self, VALUE n )
{
	TypeCheck( self, TYPE_FIXNUM );
	
	if( VALUE_TYPE(n) != TYPE_FIXNUM ) {
		OpErr( "/", self, n );
	}
	return INT2FIX( FIX2INT(self) / FIX2INT(n) );
}

// %
VALUE
rabbit_fixnum_mod( VALUE self, VALUE n )
{
	TypeCheck( self, TYPE_FIXNUM );
	
	if( VALUE_TYPE(n) != TYPE_FIXNUM ) {
		OpErr( "%", self, n );
	}
	return INT2FIX( FIX2INT(self) % FIX2INT(n) );
}

// >>
VALUE
rabbit_fixnum_right_shift( VALUE self, VALUE n )
{
	TypeCheck( self, TYPE_FIXNUM );
	
	if( VALUE_TYPE(n) != TYPE_FIXNUM ) {
		OpErr( ">>", self, n );
	}
	return INT2FIX( FIX2INT(self) >> FIX2INT(n) );
}

// <<
VALUE
rabbit_fixnum_left_shift( VALUE self, VALUE n )
{
	TypeCheck( self, TYPE_FIXNUM );
	
	if( VALUE_TYPE(n) != TYPE_FIXNUM ) {
		OpErr( "<<", self, n );
	}
	return INT2FIX( FIX2INT(self) << FIX2INT(n) );
}

// -
VALUE
rabbit_fixnum_uminus( VALUE self )
{
	TypeCheck( self, TYPE_FIXNUM );
	return INT2FIX( -FIX2INT(self) );
}


VALUE
rabbit_fixnum_putchar( VALUE self )
{
	TypeCheck( self, TYPE_FIXNUM );
	putchar( FIX2INT(self) );
	return R_NULL;
}

VALUE
rabbit_fixnum_times( VALUE self )
{
	int i, max;
	VALUE argv[1];
	
	TypeCheck( self, TYPE_FIXNUM );
	
	max = FIX2INT(self);
	for( i=0; i<max; ++i ) {
		argv[0] = INT2FIX(i);
		rabbit_yield( self, 1, argv );
	}
	return R_NULL;
}


VALUE
fixnum_to_string( VALUE num, int base )
{
	char buf[ sizeof(long) * 8 + 2 ];
	char *ptr = buf + sizeof(buf);
	int  n = FIX2INT(num);
	int sign = 0;
	
	if( base < 2 || base > 36 ) {
		rabbit_raise( "illegal radix %d", base );
	}
	if( n == 0 ) {
		return rabbit_string_new( 1, "0" );
	}
	if( n < 0 ) {
		sign = 1;
		n = -n;
	}
	
	*--ptr = '\0';
	do {
		*--ptr = rabbit_digitmap[ n % base ];
	} while( n /= base );
	
	if( sign ) {
		*--ptr = '-';
	}
	
	return rabbit_string_new( strlen(ptr), ptr );
}

VALUE
rabbit_fixnum_to_string( VALUE self, int argc, VALUE argv[] )
{
	TypeCheck( self, TYPE_FIXNUM );
	
	if( argc == 0 ) {
		return fixnum_to_string( self, 10 );
	} else {
		return fixnum_to_string( self, FIX2INT(argv[0]) );
	}
}

VALUE
rabbit_fixnum_random( VALUE self, VALUE min, VALUE max )
{
	static int flag;
	int _min, _max;
	
	TypeCheck( min, TYPE_FIXNUM );
	TypeCheck( max, TYPE_FIXNUM );
	
	_min = FIX2INT(min);
	_max = FIX2INT(max);

	if (flag == 0) {
		srand( (unsigned int)( getpid() + time(NULL) ) );
		flag = 1;
	}
	return INT2FIX( _min + (int)(rand()*( _max - _min+1.0)/(1.0+RAND_MAX)));
}

void Numeric_Setup()
{
	// Kernel > Object > Numeric
	C_Numeric = rabbit_define_class( "Numeric", C_Object );
	
	// Kernel > Object > Numeric > Fixnum
	C_Fixnum  = rabbit_define_class( "Fixnum", C_Numeric );
	
	rabbit_define_global_function( "random", rabbit_fixnum_random, 2 );

//	rabbit_undef_method( R_BASIC(C_Fixnum)->klass, "new" );
	rabbit_undef_method( CLASS_OF(C_Fixnum), "new" );

	rabbit_define_method( C_Fixnum, "==", rabbit_fixnum_eq, 1 );
	rabbit_define_method( C_Fixnum, "!=", rabbit_fixnum_ne, 1 );
	rabbit_define_method( C_Fixnum, ">", rabbit_fixnum_gt, 1 );
	rabbit_define_method( C_Fixnum, ">=", rabbit_fixnum_ge, 1 );
	rabbit_define_method( C_Fixnum, "<", rabbit_fixnum_lt, 1 );
	rabbit_define_method( C_Fixnum, "<=", rabbit_fixnum_le, 1 );
	rabbit_define_method( C_Fixnum, "-@", rabbit_fixnum_uminus, 0 );
	rabbit_define_method( C_Fixnum, "+", rabbit_fixnum_add, 1 );
	rabbit_define_method( C_Fixnum, "-", rabbit_fixnum_sub, 1 );
	rabbit_define_method( C_Fixnum, "*", rabbit_fixnum_mul, 1 );
	rabbit_define_method( C_Fixnum, "/", rabbit_fixnum_div, 1 );
	rabbit_define_method( C_Fixnum, "%", rabbit_fixnum_mod, 1 );
	rabbit_define_method( C_Fixnum, ">>", rabbit_fixnum_right_shift, 1 );
	rabbit_define_method( C_Fixnum, "<<", rabbit_fixnum_left_shift, 1 );
	rabbit_define_method( C_Fixnum, "putchar", rabbit_fixnum_putchar, 0 );
	rabbit_define_method( C_Fixnum, "times", rabbit_fixnum_times, 0 );
	rabbit_define_method( C_Fixnum, "to_s", rabbit_fixnum_to_string, -1 );
}

