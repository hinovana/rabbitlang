#include <stdio.h>
#include <string.h>
#include "rabbit.h"

#define ARRAY_MAX_SIZE      1000000
#define ARRAY_DEFAULT_SIZE  16


// -------------------------------------------------------------------
// Array class
// -------------------------------------------------------------------
VALUE rabbit_array_s_new( VALUE self, int argc, VALUE *argv )
{
	volatile struct RArray *arr;

	if( self == R_NULL ) {
		self = C_Array;
	}
	
	RABBIT_OBJECT_NEW( arr, struct RArray );
	RABBIT_OBJECT_SETUP( arr, self, TYPE_ARRAY );
	{
		arr->capa  = ARRAY_DEFAULT_SIZE;
		arr->len   = 0;
		arr->ptr   = (VALUE*)rabbit_malloc( sizeof(VALUE) * ARRAY_DEFAULT_SIZE );
	}

	return (VALUE)arr;
}

VALUE rabbit_array_new()
{
	return rabbit_array_s_new( C_Array, 0, 0 );
}


VALUE rabbit_array_at( VALUE self, VALUE pos )
{
	if( VALUE_TYPE(self) != TYPE_ARRAY ) {
		rabbit_raise( "type error - !rabbit_array_at type(%d)", VALUE_TYPE(self) );
	}
	struct RArray *arr = R_ARRAY(self);
	
	if( arr->len == 0 ) {
		return R_NULL;
	}
	if( !IS_FIXNUM_VALUE(pos) || FIX2INT(pos) >= arr->len ) {
		return R_NULL;
	}
	return arr->ptr[ FIX2INT(pos) ];
}

VALUE rabbit_array_len( VALUE self )
{
	if( VALUE_TYPE(self) != TYPE_ARRAY ) {
		fprintf( stderr, "type error - ?rabbit_array_len\n" );
		abort();
	}
	
	return INT2FIX( R_ARRAY(self)->len);
}

VALUE rabbit_array_each( VALUE self )
{
	int i;
	VALUE argv[1];

	assert(self);

	if( VALUE_TYPE(self) != TYPE_ARRAY ) {
		fprintf( stderr, "type error - rabbit_array_each\n" );
		abort();
	}
	
	for( i=0; i < R_ARRAY(self)->len; ++i ) {
		argv[0] = R_ARRAY(self)->ptr[i];
		rabbit_yield( self, 1, argv );
	}
	return R_NULL;
}

VALUE rabbit_array_each_with_index( VALUE self )
{
	int i;
	VALUE argv[2];

	assert(self);

	if( VALUE_TYPE(self) != TYPE_ARRAY ) {
		fprintf( stderr, "type error - rabbit_array_each_with_index\n" );
		abort();
	}
	
	for( i=0; i < R_ARRAY(self)->len; ++i ) {
		argv[0] = R_ARRAY(self)->ptr[i];
		argv[1] = INT2FIX(i);
		rabbit_yield( self, 2, argv );
	}
	return R_NULL;
}

VALUE rabbit_array_set( VALUE self, VALUE slot_num, VALUE operand )
{
	struct RArray* arr;
	
	if( VALUE_TYPE(self) != TYPE_ARRAY ) {
		rabbit_bug2( "[bug] selfの型がRArrayじゃないです" );
	}
	if( VALUE_TYPE(slot_num) != TYPE_FIXNUM || FIX2INT(slot_num) < 0  ) {
		rabbit_raise2( "スロット番号の指定がおかしいです" );
	}
	if( FIX2INT(slot_num) >= ARRAY_MAX_SIZE ) {
		rabbit_raise( "ARRAY_MAX_SIZEは'%d'です。", ARRAY_MAX_SIZE  );
	}
	
	arr = R_ARRAY(self);
	
	// 添え字番号が今の配列のサイズ内に収まっている
	if( R_ARRAY(self)->len > FIX2INT(slot_num) ) {
		R_ARRAY(self)->ptr[ FIX2INT(slot_num) ] = operand;
		return operand;
	}
	
	// capaのサイズを超えている
	if( arr->capa <= FIX2INT(slot_num) ) {
		int lack_len = FIX2INT(slot_num) - arr->capa;
		int new_capa = arr->capa + lack_len + ARRAY_DEFAULT_SIZE;

		arr->ptr  = rabbit_realloc( arr->ptr, sizeof(VALUE) * new_capa );
		arr->capa = new_capa;
	}

	int i = arr->len;
	for( ; i < FIX2INT(slot_num); ++i ) {
		// nullで埋める
		arr->ptr[i] = R_NULL;
	}
	arr->ptr[ FIX2INT(slot_num) ] = operand;
	arr->len = FIX2INT(slot_num) + 1;

	return operand;
}

VALUE rabbit_array_push( VALUE self, int argc, VALUE *argv )
{
	struct RArray *arr = R_ARRAY(self);
	int i;

	if( arr->capa <= (arr->len + argc) ) {
		int new_size = (arr->capa + argc + ARRAY_DEFAULT_SIZE) * sizeof(VALUE);
		arr->ptr  = (VALUE*)rabbit_realloc( arr->ptr, new_size );
		arr->capa = arr->capa + argc + ARRAY_DEFAULT_SIZE;
	}

	for( i=0; i<argc; i++ ) {
		arr->ptr[ arr->len ] = argv[i];
		arr->len++;
	}

	return (VALUE)arr;
}

VALUE rabbit_array_pop( VALUE self )
{
	VALUE result;
	struct RArray *arr = R_ARRAY(self);
	
	if( arr->len == 0 ) {
		return R_NULL;
	}
	result = arr->ptr[ arr->len - 1 ];

	arr->len--;
	
	return result;
}


VALUE rabbit_array_shift( VALUE self )
{
	VALUE top;
	struct RArray *arr = R_ARRAY(self);

	if( arr->len == 0 ) {
		return R_NULL;
	}

	top = *(arr->ptr);
	
	arr->len--;
//	arr->capa--;

	memmove( arr->ptr, arr->ptr+1, sizeof(VALUE) * arr->len );
	
	return top;
}

VALUE rabbit_array_clear( VALUE self )
{
	struct RArray *arr = R_ARRAY(self);
	free( arr->ptr );

	arr->ptr = 0;
	arr->capa = 0;
	arr->len  = 0;

	return R_NULL;
}

VALUE rabbit_array_to_string( VALUE self )
{
	VALUE result;
	char* ress = malloc(1);
	int len = FIX2INT( rabbit_array_len( self ) );
	int i;

	ress[0] = '\0';

#define ress_concat( str_ ) do{										\
	ress = realloc( ress, strlen(ress) + strlen(str_) + 2 );		\
	strcat( ress, str_ );											\
	strcat( ress, "," );											\
} while(0)

	ress = realloc( ress, strlen(ress) + 1 );
	strcat( ress, "[" );
	
	for( i=0; i<len; ++i )
	{
		char buff[100];
		VALUE val;
		
		if( VALUE_TYPE(self) != TYPE_ARRAY ) {
			rabbit_bug2("Array#to_sの最中に GCが起きて悲しいことになりました。\n対象のオブジェクトを一旦変数に入れるといいかもです。");
		}
		val = rabbit_array_at( self, INT2FIX(i) );
		
		switch( VALUE_TYPE(val) )
		{
		case TYPE_TRUE:
			ress_concat( "true" );
			break;
		case TYPE_FALSE:
			ress_concat( "false" );
			break;
		case TYPE_NULL:
			ress_concat( "null" );
			break;
		case TYPE_FIXNUM:
			sprintf( buff, "%d", FIX2INT(val) );
			ress_concat( buff );
			break;
		case TYPE_STRING:
		STR_SET:
			ress_concat( R_STRING(val)->ptr );
			break;
		case TYPE_ARRAY:
			val = rabbit_array_to_string( val );
			goto STR_SET;
		case TYPE_CLASS:
			sprintf( buff, "CLASS(%p)", (void*)val );
			ress_concat( buff );
			break;
		case TYPE_OBJECT:
			sprintf( buff, "OBJECT(%p)", (void*)val );
			ress_concat( buff );
			break;
		default:
			sprintf( buff, "(type:%d)", VALUE_TYPE(val) );
			ress_concat( buff );
		}
		
	}

	if( strlen(ress) > 1 ) {
		ress[ strlen(ress) - 1 ] = '\0';
	}

	ress = realloc( ress, strlen(ress) + 1 );
	strcat( ress, "]" );

#undef ress_concat

	result = rabbit_string_new( strlen(ress), ress );
	free(ress);

	return result;
}

void Array_Setup()
{
	// Objectを継承したArrayクラスの作成
	C_Array = rabbit_define_class( "Array", C_Object );

	// Kernel -> Object -> Array
	
	// メソッドの登録
	rabbit_define_singleton_method( C_Array, "new", rabbit_array_s_new, -1 );

	rabbit_define_method( C_Array, "push", rabbit_array_push, -1 );
	rabbit_define_method( C_Array, "shift", rabbit_array_shift, 0 );
	rabbit_define_method( C_Array, "at", rabbit_array_at, 1 );
	rabbit_define_method( C_Array, "[]", rabbit_array_at, 1 );
	rabbit_define_method( C_Array, "[]=", rabbit_array_set, 2 );
	rabbit_define_method( C_Array, "len", rabbit_array_len, 0 );
	rabbit_define_method( C_Array, "length", rabbit_array_len, 0 );
	rabbit_define_method( C_Array, "clear", rabbit_array_clear, 0 );
	rabbit_define_method( C_Array, "pop", rabbit_array_pop, 0 );
	rabbit_define_method( C_Array, "to_s", rabbit_array_to_string, 0 );
	rabbit_define_method( C_Array, "each", rabbit_array_each, 0 );
	rabbit_define_method( C_Array, "each_with_index", rabbit_array_each_with_index, 0 );
}


