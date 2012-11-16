#include "rabbit.h"
#include "node.h"
#include "st.h"

// ------------------------------------------------------------------
// klass utils
// ------------------------------------------------------------------
VALUE rabbit_class_new( VALUE super )
{
	struct RClass *klass;

	RABBIT_OBJECT_NEW( klass, struct RClass );
	RABBIT_OBJECT_SETUP( klass, C_Class, TYPE_CLASS );
	{
		klass->method_tbl   = st_init_numtable();
		klass->ival_tbl     = 0;
		klass->super        = ( super == R_NULL ) ? R_NULL : R_CLASS(super);
		klass->symbol       = 0;
	}
	return (VALUE)klass;
}

VALUE rabbit_singleton_class_new( VALUE super )
{
	VALUE klass = rabbit_class_new( super );
	
	FLAG_SET(klass,FLAG_SINGLETON);

	return klass;
}

VALUE rabbit_define_class( const char* name, VALUE super )
{
	extern struct st_table *class_tbl;
	VALUE klass;

	if( (klass = rabbit_class_lookup( rabbit_name2tag(name) )) != R_NULL ) {
		if( super && (VALUE)(R_CLASS(klass)->super) != super ) {
			fprintf( stderr, "superclass missmatch for %s (TypeError)\n", name );
			abort();
		}
		return klass;
	}

	if( super == R_NULL ) {
		super = C_Object;
	}

	klass = rabbit_class_new( super );

	R_BASIC(klass)->klass = rabbit_singleton_class_new( R_BASIC(super)->klass );
	R_CLASS(klass)->symbol = rabbit_name2tag(name);

	st_insert( class_tbl, R_CLASS(klass)->symbol, klass );

	return klass;
}

VALUE rabbit_class_lookup( TAG symbol )
{
	extern struct st_table *class_tbl;
	VALUE klass;
	
	if( st_lookup( class_tbl, symbol, (st_data_t*)&klass ) == 0 ) {
		return (VALUE)R_NULL;
	}
	return klass;
}


VALUE rabbit_singleton_class( VALUE object )
{
	if( FLAG_TEST( R_BASIC(object)->klass, FLAG_SINGLETON ) ) {
		return R_BASIC(object)->klass;
	}
	R_BASIC(object)->klass = rabbit_singleton_class_new( R_BASIC(object)->klass );

	return R_BASIC(object)->klass;
}


void rabbit_define_method( VALUE klass, const char* name, VALUE (*cfunction)(), int arg_len )
{
	TAG symbol = rabbit_name2tag(name);
	struct RMethod *method;

	RABBIT_OBJECT_NEW( method, struct RMethod );
	RABBIT_OBJECT_SETUP( method, klass, TYPE_METHOD );
	{
		method->func_type   = FUNCTION_E_C;
		method->u.cfunc_ptr = cfunction;
		method->arg_len     = arg_len;
	}

	st_insert( R_CLASS(klass)->method_tbl, symbol, (st_data_t)method );
}

void rabbit_define_method_node( VALUE klass, const char* name, struct RNodeFunction* func_node )
{
	TAG symbol = rabbit_name2tag(name);
	struct RMethod *method;

	RABBIT_OBJECT_NEW( method, struct RMethod );
	RABBIT_OBJECT_SETUP( method, klass, TYPE_METHOD );
	{
		method->func_type   = FUNCTION_E_RABBIT;
		method->u.func_node = func_node;
		method->arg_len     = -1;
	}
	
	st_insert( R_CLASS(klass)->method_tbl, (st_data_t)symbol, (st_data_t)method );
}

void rabbit_define_singleton_method( VALUE object, const char* name, VALUE (*cfunction)(), int arg_len )
{
	rabbit_define_method( rabbit_singleton_class( object ), name, cfunction, arg_len );
}

void rabbit_define_global_function( const char* name, VALUE (*cfunction)(), int arg_len )
{
	rabbit_define_singleton_method( C_Kernel, name, cfunction, arg_len );
	rabbit_define_method( C_Kernel, name, cfunction, arg_len );
}

void rabbit_undef_method( VALUE klass, const char* name )
{
	TAG symbol = rabbit_name2tag(name);
	struct RMethod *method;

	RABBIT_OBJECT_NEW( method, struct RMethod );
	RABBIT_OBJECT_SETUP( method, klass, TYPE_METHOD );
	{
		method->func_type   = FUNCTION_E_UNDEF;
		method->u.cfunc_ptr = 0;
		method->arg_len     = 0;
	}
	st_insert( R_CLASS(klass)->method_tbl, symbol, (st_data_t)method );
}




