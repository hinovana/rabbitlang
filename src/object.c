#include "rabbit.h"
#include "node.h"
#include "version.h"
#include "st.h"

struct st_table *class_tbl;

VALUE rabbit_class_new( VALUE super );
VALUE rabbit_singleton_class_new( VALUE super );
VALUE call_rabbit_func( VALUE klass, VALUE self, NODE func_node, int argc, VALUE *argv, VALUE parent_scope );
VALUE call_c_func( VALUE klass, VALUE object, VALUE method, int argc, VALUE *argv );

// ------------------------------------------------------------------
// ------------------------------------------------------------------
void rabbit_set_instance_value( VALUE self, const TAG symbol, VALUE value )
{
	assert( self );
	assert( symbol );
	
	if( VALUE_TYPE(self) != TYPE_OBJECT ) {
		fprintf( stderr, "type error - rabbit_set_instance_value - object type \n" );
		abort();
	}
	
	// まだハッシュテーブルが作られていなかったら、作る
	if( R_OBJECT(self)->ival_tbl == 0 ) {
		R_OBJECT(self)->ival_tbl = st_init_numtable();
	}
	
	assert( R_OBJECT(self)->ival_tbl );

	// ハッシュテーブルにセットする
	st_insert( R_OBJECT(self)->ival_tbl, symbol, (st_data_t)value );
}

VALUE rabbit_get_instance_value( VALUE self, const TAG symbol, VALUE *value )
{
	assert( self );
	assert( symbol );
	
	if( VALUE_TYPE(self) != TYPE_OBJECT ) {
		fprintf( stderr, "type error - rabbit_get_instance_value - object type \n" );
		abort();
	}

	if( R_OBJECT(self)->ival_tbl == 0 ) {
		return R_NULL;
	}
	
	// ハッシュテーブルから取得する
	if( st_lookup( R_OBJECT(self)->ival_tbl, symbol, (st_data_t*)value ) == 0 ) {
		// 無かった
		return R_NULL;
	}
	return R_TRUE;
}


// ------------------------------------------------------------------
// Class methods
// ------------------------------------------------------------------
// Class.new()
// 引数はsuperクラス
VALUE rabbit_class_s_new( struct RClass *self, int argc, VALUE *argv )
{
	VALUE klass;
	VALUE super;

	assert( self );
	super = (argc == 0) ? C_Object : argv[0];
	
	if( VALUE_TYPE( super ) != TYPE_CLASS ) {
		fprintf( stderr, "[bug] super klass lost\n" );
		abort();
	}
	
	// スーパークラスがシングルトンじゃない
	if( !FLAG_TEST( R_BASIC(super)->klass, FLAG_SINGLETON ) ) {
		fprintf( stderr, "[bug] super klass is not singleton class2!!\n" );
		abort();
	}
	klass = rabbit_class_new( super );
	R_BASIC(klass)->klass = rabbit_singleton_class_new( R_BASIC(super)->klass );

	return klass;
}


// Class.new().new()
VALUE rabbit_class_new_instance( VALUE self, int argc, VALUE* argv )
{
	struct RObject *object;
	VALUE init_method;
	VALUE klass = self;

	assert(klass);

	if( FLAG_TEST( self, FLAG_SINGLETON ) ) {
		rabbit_bug2( "シングルトンクラスのインスタンスは作れません" );
	}

	RABBIT_OBJECT_NEW( object, struct RObject );
	RABBIT_OBJECT_SETUP( object, klass, TYPE_OBJECT );
	{
		object->ival_tbl   = st_init_numtable();
	}
	
	if( (init_method = class_method_lookup( R_CLASS(R_BASIC(object)->klass), rabbit_tag_initialize )) != R_NULL ) {
		if( R_METHOD( init_method )->func_type == FUNCTION_E_C ) {
			assert( rabbit_tag_initialize );
			class_method_call( klass, (VALUE)object, rabbit_tag_initialize, argc, argv );
		} else {
			NODE func_node = (NODE)(R_METHOD( init_method )->u.func_node);
			call_rabbit_func( klass, (VALUE)object, func_node, argc, argv, (VALUE)NULL );
		}
	}
	return (VALUE)object;
}


VALUE class_method_lookup( struct RClass *klass, TAG symbol )
{
	VALUE result = R_NULL;
	VALUE method;
	struct RClass *super;

	assert( klass );
	assert( symbol );
	assert( R_BASIC(klass)->klass );

	super = klass;

	while( super != NULL ) {
		if( st_lookup( super->method_tbl, symbol, &method ) != 0 ) {
			result = method;
			break;
		}
		super = super->super;
	}
	if( result == R_NULL || R_METHOD(result)->func_type == FUNCTION_E_UNDEF ) {
		return R_NULL;
	}
	return result;
}


VALUE class_method_call( VALUE klass, VALUE object, TAG symbol, int argc, VALUE *argv )
{
	VALUE method = class_method_lookup( R_CLASS(klass), symbol );

	if( method == R_NULL ) {
		// メソッドが無い
		fprintf( stderr, "'%s' method not found.. - %s:%d\n", rabbit_tag2name(symbol), __FILE__, __LINE__ );
		abort();
	}
	return call_c_func( klass, object, method, argc, argv );
}


struct st_table *ignore_methods_tbl_;

static int __rabbit_class_method_each( TAG tag, VALUE method, VALUE *arr )
{
	if( st_lookup( ignore_methods_tbl_, tag, 0 ) ) {
		return ST_CONTINUE;
	}
	if( R_METHOD(method)->func_type != FUNCTION_E_UNDEF ) {
		char* ptr = rabbit_tag2name(tag);

		VALUE vars[1] = { rabbit_string_new( strlen(ptr), ptr ) };
		rabbit_array_push( *arr, 1, vars );
	}
	st_add_direct( ignore_methods_tbl_, tag, 0 );

	return ST_CONTINUE;
}

VALUE rabbit_object_methods( VALUE self )
{
	VALUE klass, super, result;
	
	klass = CLASS_OF(self);
	
	if( klass == R_NULL ) {
		fprintf( stderr, "[m(_ _)m]まだそのクラスは作っていません(%d) - %s:%d\n", VALUE_TYPE(self), __FILE__, __LINE__ );
		abort();
	}
	super  = klass;
	result = rabbit_array_new();
	
	ignore_methods_tbl_ = st_init_numtable();

	while( super != R_NULL ) {
		st_foreach( R_CLASS(super)->method_tbl, __rabbit_class_method_each, (st_data_t)&result );
		super = (VALUE)( R_CLASS(super)->super );
	}
	
	st_free_table( ignore_methods_tbl_ );
	ignore_methods_tbl_ = 0;
	
	return result;
}


VALUE rabbit_class_instance_method_list( VALUE self )
{
	VALUE super, result;
	
	if( !IS_PTR_VALUE(self) ) {
		fprintf( stderr, "まだ動作を考えてません！\n" );
		abort();
	}

	super = self;
	result = rabbit_array_new();
	
	ignore_methods_tbl_ = st_init_numtable();

	while( super != R_NULL ) {
		st_foreach( R_CLASS(super)->method_tbl, __rabbit_class_method_each, (st_data_t)&result );
		super = (VALUE)( R_CLASS(super)->super );
	}
	
	st_free_table( ignore_methods_tbl_ );
	ignore_methods_tbl_ = 0;
	
	return result;
}


VALUE boot_define_class( const char* name, VALUE super )
{
	VALUE obj  = rabbit_class_new( super );
	
	R_CLASS(obj)->symbol = rabbit_name2tag(name);
	st_insert( class_tbl, R_CLASS(obj)->symbol, obj );
	
	return obj;
}


VALUE rabbit_f_exit( VALUE self, int argc, VALUE *argv )
{
	fprintf( stderr, "Kernel.exit!\n" );
	exit(0);
}

VALUE rabbit_f_print( VALUE self, int argc, VALUE *argv )
{
	int i ;

	if( argc < 1 ) {
		return R_NULL;
	}
	
	for( i=0; i<argc; ++i ) {
		switch( VALUE_TYPE(argv[i]) )
		{
		case TYPE_TRUE:
			fprintf( stdout, "true" );
			break;
		case TYPE_FALSE:
			fprintf( stdout, "false" );
			break;
		case TYPE_NULL:
			fprintf( stdout, "null" );
			break;
		case TYPE_FIXNUM:
			fprintf( stdout, "%d", FIX2INT(argv[i]) );
			break;
		case TYPE_STRING:
			fprintf( stdout, "%s", R_STRING(argv[i])->ptr );
			break;
		case TYPE_ARRAY: {
			VALUE out = rabbit_array_to_string( argv[i] );
			fprintf( stdout, "%s", R_STRING(out)->ptr );
			break;
		}
		case TYPE_CLASS:
			fprintf( stdout, "CLASS(%s:%p)", rabbit_tag2name(R_CLASS(argv[i])->symbol), (void*)(argv[i]) );
			break;
		case TYPE_OBJECT:
			fprintf( stdout, "OBJECT(%p)", (void*)(argv[i]) );
			break;
		default:
			fprintf( stderr, "(no support type %d)", VALUE_TYPE(argv[i]) );
		}
	}
	
	return argv[ argc - 1 ];
}


VALUE rabbit_f_puts( VALUE self, int argc, VALUE *argv )
{
	VALUE result = rabbit_f_print( self, argc, argv );
	fprintf( stdout, "\n");
	
	return result;
}

VALUE rabbit_object_eq( VALUE self, VALUE obj )
{
	return self == obj ? R_TRUE : R_FALSE;
}

VALUE rabbit_object_ne( VALUE self, VALUE obj )
{
	return self != obj ? R_TRUE : R_FALSE;
}

VALUE rabbit_object_class_name( VALUE self )
{
	return CLASS_OF(self);
}


void Object_Setup()
{
	class_tbl = st_init_numtable();
	
	C_Class = 0;
	
	// Kernel, Object, Classをブートする
	C_Kernel = boot_define_class( "Kernel", R_NULL );
	C_Object = boot_define_class( "Object", C_Kernel );
	C_Class  = boot_define_class( "Class",  C_Object );
	
	{ // meta class
		VALUE metaclass;

		metaclass = R_BASIC(C_Kernel)->klass = rabbit_singleton_class_new( C_Class );
		metaclass = R_BASIC(C_Object)->klass = rabbit_singleton_class_new( metaclass );
		metaclass = R_BASIC(C_Class)->klass  = rabbit_singleton_class_new( metaclass );
	}

	{ // Class class
		// class Hoge のとき呼ばれる
		rabbit_define_singleton_method( C_Class, "new", rabbit_class_s_new, -1 );

		// Hoge.new のとき呼ばれる
		rabbit_define_method( C_Class, "new", rabbit_class_new_instance, -1 );
	}
	
	{ // Object class
		rabbit_define_method( C_Object, "==", rabbit_object_eq, 1 );
		rabbit_define_method( C_Object, "!=", rabbit_object_ne, 1 );

		rabbit_define_method( C_Object, "methods", rabbit_object_methods, 0 );
		rabbit_define_method( C_Object, "class", rabbit_object_class_name, 0 );

		rabbit_define_singleton_method( C_Object, "instance_methods", rabbit_class_instance_method_list, 0 );
	}
	
	{ // Global function
		rabbit_define_global_function( "print", rabbit_f_print, -1 );
		rabbit_define_global_function( "puts", rabbit_f_puts, -1 );
		rabbit_define_global_function( "exit!", rabbit_f_exit, -1 );
		rabbit_define_global_function( "version", rabbit_f_get_version, 0 );
	}

	{ // Boolean
		C_TrueClass = rabbit_define_class( "True", C_Object );
		rabbit_undef_method( R_BASIC(C_TrueClass)->klass, "new" );
		
		C_FalseClass = rabbit_define_class( "False", C_Object );
		rabbit_undef_method( R_BASIC(C_FalseClass)->klass, "new" );
	}
}

