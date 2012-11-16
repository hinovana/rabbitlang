/* The Rabbit programming language 
 * http://code.google.com/p/rabbitlang/
 *
 * Copyright (c) 2010, HINOVANA<hinovana@gmail.com>
 * All rights reserved.
 *
 */
#ifndef RABBIT_H_
#define RABBIT_H_
#if defined(__cplusplus)
extern "C" {
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "config.h"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#   define YY_NO_UNISTD_H 1
#endif

#ifndef UINT
#   define UINT unsigned int
#endif
#ifndef ULONG
#   define ULONG unsigned long
#endif

#ifdef HAVE_LIMITS_H
#   include <limits.h>
#else
#   define LONG_MAX 2147483647
#endif
#ifndef LONG_MIN
#   define LONG_MIN (-LONG_MAX-1)
#endif

typedef ULONG VALUE;
typedef ULONG TAG;
typedef ULONG FLAG;

TAG rabbit_name2tag( const char* name );
char* rabbit_tag2name( TAG tag );

#define RABBIT_STACK_OK      0
#define RABBIT_STACK_FULL    1
#define RABBIT_STACK_END     2

enum rabbit_symbol_flag {
	SYMBOL_E_GLOBAL = 1,
	SYMBOL_E_IVAL   = 2,
	SYMBOL_E_LOCAL  = 3,
};

#define TAG_FLAG_GLOBAL  1
#define TAG_FLAG_IVAL    2
#define TAG_FLAG(tag_) ( \
	  (tag_ & TAG_FLAG_GLOBAL) ? SYMBOL_E_GLOBAL \
	: (tag_ & TAG_FLAG_IVAL)   ? SYMBOL_E_IVAL   \
	: SYMBOL_E_LOCAL \
)

TAG rabbit_tag_initialize;
TAG rabbit_tag_self;

// ------------------------------------------------------------------
// VALUE
// ------------------------------------------------------------------
#define TYPE_NONE     0x0
#define TYPE_OBJECT   0x1
#define TYPE_NULL     0x2
#define TYPE_TRUE     0x3
#define TYPE_FALSE    0x4
#define TYPE_FIXNUM   0x5
#define TYPE_STRING   0x6
#define TYPE_ARRAY    0x7
#define TYPE_HASH     0x8
#define TYPE_SCOPE    0x9
#define TYPE_CLASS    0xA
#define TYPE_METHOD   0xB

enum rabbit_function_type
{
	FUNCTION_E_RABBIT	= 1,
	FUNCTION_E_C		= 2,
	FUNCTION_E_UNDEF	= 3,
};

/**
 *  FLAG(32bit)
 *  MSB --------------------------- LSB
 *  ******** ******** ******** *MMMMMMM -- TYPE_MASK
 *  ******** ******** ******** F******* -- FLAG_GC_MARK
 *  ******** ******** *******F ******** -- FLAG_FINALIZE
 *  ******** ******** ******F* ******** -- FLAG_TAINT
 *  ******** ******** *****F** ******** -- FLAG_EXIVAR
 *  ******** ******** ****F*** ******** -- FLAG_SINGLETON
 *  ******** ******** ***F**** ******** -- FLAG_LITERAL
 */
#define TYPE_MASK      (0x7f)
#define FLAG_GC_MARK   (1<<7)
#define FLAG_FINALIZE  (1<<8)
#define FLAG_TAINT     (1<<9)
#define FLAG_EXIVAR    (1<<10)
#define FLAG_SINGLETON (1<<11)
#define FLAG_LITERAL   (1<<12)

#define FLAG_TEST(obj_,flg_)  ((!IS_PTR_VALUE(obj_)) ? 0 : (R_BASIC(obj_)->flags & (flg_)))
#define FLAG_SET(obj_,flg_)   if(IS_PTR_VALUE(obj_)){ R_BASIC(obj_)->flags |= (flg_); }
#define FLAG_UNSET(obj_,flg_) if(IS_PTR_VALUE(obj_)){ R_BASIC(obj_)->flags &= ~(flg_); }

struct RBasic {
	FLAG  flags;
	VALUE klass;
};

struct RClass {
	struct RBasic basic;
	struct st_table *method_tbl;
	struct st_table *ival_tbl;
	struct RClass *super;
	TAG  symbol;
};

struct RObject {
	struct RBasic basic;
	struct st_table *ival_tbl;
};

struct RString {
	struct RBasic basic;
	size_t len;
	char* ptr;
};

struct RArray {
	struct RBasic basic;
	UINT capa;
	UINT len;
	VALUE *ptr;
};


typedef VALUE (*rabbit_cfunc)();

struct RMethod {
	struct RBasic basic;
	enum rabbit_function_type func_type;
	union {
		rabbit_cfunc cfunc_ptr;
		struct RNodeFunction *func_node;
	} u;
	int arg_len;
};

struct RScope {
	struct RBasic basic;
	VALUE  parent;
	struct st_table *var_tbl;
	VALUE  klass;
	int    is_used;
};


#define FIXNUM_FLAG  0x01

#if (-1==(((-1)<<1)&FIXNUM_FLAG)>>1)
#    define RSHIFT(x,y) ((x)>>y)
#else
#    define RSHIFT(x,y) (((x)<0) ? ~((~(x))>>y) : (x)>>y)
#endif

#define FIX2INT(x)  RSHIFT((int)x,1)
#define INT2FIX(i)  ((((long)(i)) << 1 | FIXNUM_FLAG))

#define R_FALSE  0x0
#define R_TRUE   0x2
#define R_NULL   0x0

#define R_CAST(T)             (struct T *)
#define R_BASIC(v)            (R_CAST(RBasic)v)
#define R_CLASS(v)            (R_CAST(RClass)v)
#define R_HASH(v)             (R_CAST(RHash)v)
#define R_FUNC(v)             (R_CAST(RFunction)v)
#define R_STRING(v)           (R_CAST(RString)v)
#define R_CLASS(v)            (R_CAST(RClass)v)
#define R_ARRAY(v)            (R_CAST(RArray)v)
#define R_OBJECT(v)           (R_CAST(RObject)v)
#define R_METHOD(v)           (R_CAST(RMethod)v)
#define R_SCOPE(v)            (R_CAST(RScope)v)

#define IS_FIXNUM_VALUE(v)    (((long)v & 1))
#define IS_TRUE_VALUE(v)      ((VALUE)v == R_TRUE)
#define IS_FALSE_VALUE(v)     ((VALUE)v == R_FALSE)
#define IS_NULL_VALUE(v)      ((VALUE)v == R_NULL)

#define IS_PTR_VALUE(v) (  \
	!IS_FIXNUM_VALUE(v) && \
	!IS_TRUE_VALUE(v)   && \
	!IS_FALSE_VALUE(v)  && \
	!IS_NULL_VALUE(v)      \
)

#define IS_FALSE(v) (IS_FALSE_VALUE(v)||IS_NULL_VALUE(v))
#define IS_TRUE(v)  (!IS_FALSE(v))

#define BUILTIN_TYPE(v) ((int)(((struct RBasic*)(v))->flags & TYPE_MASK))

#define VALUE_TYPE(v) ( \
	  IS_FIXNUM_VALUE(v) ? TYPE_FIXNUM	\
	: IS_FALSE_VALUE(v)  ? TYPE_FALSE	\
	: IS_TRUE_VALUE(v)   ? TYPE_TRUE	\
	: IS_NULL_VALUE(v)   ? TYPE_NULL	\
	: BUILTIN_TYPE(v)					\
)

#define VALUE_TYPE_EQ(l,r)    (VALUE_TYPE(lval) == VALUE_TYPE(rval))

#define CLASS_OF(obj_) ( \
	  IS_PTR_VALUE(obj_) ? R_BASIC(obj_)->klass \
	: VALUE_TYPE(obj_) == TYPE_FIXNUM ? C_Fixnum \
	: VALUE_TYPE(obj_) == TYPE_FALSE  ? C_FalseClass \
	: VALUE_TYPE(obj_) == TYPE_TRUE   ? C_TrueClass \
	: R_NULL \
)

// ------------------------------------------------------------------
// Class
// ------------------------------------------------------------------

VALUE C_Kernel;
VALUE C_Object;
VALUE C_Class;
VALUE C_Array;
VALUE C_String;
VALUE C_Numeric;
VALUE C_Fixnum;
VALUE C_TrueClass;
VALUE C_FalseClass;
VALUE C_Gc;

void Init_Stack( VALUE *ptr );

void Object_Setup();
void Array_Setup();
void Scope_Setup();
void Symbol_Setup();
void Numeric_Setup();
void String_Setup();
void Gc_Setup();
void Heap_Setup();
void Init_Lexer( FILE *lexer_in );

extern struct st_table *class_tbl;

// ------------------------------------------------------------------
// Error
// ------------------------------------------------------------------
#define RABBIT_ERROR_HEAD  "[(;_;)] Rabbit error"
#define RABBIT_BUG_HEAD    "[m(_ _)m] Rabbit BUG"

#define rabbit_raise(fmt,...) \
	rabbit_raise_f(RABBIT_ERROR_HEAD,__FILE__,__LINE__,fmt,__VA_ARGS__)
#define rabbit_raise2(fmt) \
	rabbit_raise_f(RABBIT_ERROR_HEAD, __FILE__, __LINE__, fmt )
#define rabbit_bug(fmt,...) \
	rabbit_raise_f(RABBIT_BUG_HEAD, __FILE__, __LINE__, fmt, __VA_ARGS__ )
#define rabbit_bug2(fmt) \
	rabbit_raise_f(RABBIT_BUG_HEAD, __FILE__, __LINE__, fmt )

#define OpErr(op_,x_,y_) do { \
	rabbit_raise( "opration error VALUE_TYPE(%d) %s VALUE_TYPE(%d)", VALUE_TYPE(x_), op_, VALUE_TYPE(y_) ); \
} while(0)

#define TypeErr(type_) do { \
	rabbit_raise( "type error VALUE_TYPE(%d)", type_ ); \
} while(0)

#define TypeCheck(obj_,type_) do { \
	if( VALUE_TYPE(obj_) != type_ ) { \
		TypeErr(type_); \
	} \
} while(0)


void rabbit_raise_f( const char* head, const char* c_file, int c_line, const char *fmt, ...);


// ------------------------------------------------------------------
// SCOPE
// ------------------------------------------------------------------

enum scope_search_type {
	SCOPE_SEARCH_E_ONLY,
	SCOPE_SEARCH_E_CHAIN,
};

VALUE scope_new( VALUE parent_scope );
void scope_dispose( VALUE scope );
int scope_get_value( VALUE scope, TAG tag, enum scope_search_type search_type, VALUE *hit_scope, VALUE *hit_value );
int scope_set_value( VALUE scope, TAG tag, VALUE value );
VALUE scope_class_get( VALUE scope );
void scope_class_set( VALUE scope, VALUE klass );
void scope_values_each( VALUE scope, void (*callback)(TAG,VALUE) );
void active_scope_foreach( void (*callback)(VALUE) );


// ------------------------------------------------------------------
// Garbage Collection
// ------------------------------------------------------------------
void *rabbit_malloc( size_t size );
void *rabbit_realloc( void* ptr, size_t size );
void *rabbit_calloc( size_t n, size_t size );

VALUE rabbit_gc_new_object();
void rabbit_gc_global( VALUE *value );
void rabbit_gc_literal( VALUE value );


// ------------------------------------------------------------------
// API
// ------------------------------------------------------------------

#define RABBIT_OBJECT_NEW(obj,vtype)   obj = (vtype*)rabbit_gc_new_object()
#define RABBIT_OBJECT_SETUP( obj_, cls_, type_ ) do { \
	R_BASIC(obj_)->flags = type_; \
	R_BASIC(obj_)->klass = cls_; \
} while(0)

// utils
VALUE rabbit_yield( VALUE self, int argc, VALUE *argv );

// Class utils
VALUE rabbit_define_class( const char* name, VALUE super );
VALUE rabbit_class_lookup( TAG symbol );
void rabbit_define_global_function( const char* name, VALUE (*cfunction)(), int arg_len );
void rabbit_define_method( VALUE klass, const char* name, VALUE (*cfunction)(), int arg_len );
void rabbit_define_method_node( VALUE klass, const char* name, struct RNodeFunction* func_node );
void rabbit_define_singleton_method( VALUE object, const char* name, VALUE (*cfunction)(), int arg_len );
void rabbit_undef_method( VALUE klass, const char* name );
void rabbit_set_instance_value( VALUE self, TAG symbol, VALUE value );
VALUE rabbit_get_instance_value( VALUE self, TAG symbol, VALUE *value );
VALUE class_method_call( VALUE klass, VALUE object, TAG method_symbol, int argc, VALUE *argv );
VALUE class_method_lookup( struct RClass *klass, TAG method_symbol );

// Class class methods
VALUE rabbit_class_new_instance( VALUE self, int argc, VALUE* argv );

// Object class methods
VALUE rabbit_object_class_name( VALUE self );
VALUE rabbit_object_methods( VALUE self );
VALUE rabbit_object_eq( VALUE self, VALUE obj );
VALUE rabbit_object_ne( VALUE self, VALUE obj );

// String class methods
VALUE rabbit_string_new( size_t len, char* ptr );
VALUE rabbit_string_length( VALUE self );
VALUE rabbit_string_add( VALUE self, VALUE right );
VALUE rabbit_string_each_byte( VALUE self );
VALUE rabbit_string_to_integer( VALUE self );
VALUE rabbit_string_eval( VALUE self );
VALUE rabbit_string_eq( VALUE self, VALUE s );
VALUE rabbit_string_ne( VALUE self, VALUE s );

// Array class methods
VALUE rabbit_array_new();
VALUE rabbit_array_set( VALUE self, VALUE slot_num, VALUE operand );
VALUE rabbit_array_push( VALUE self, int argc, VALUE *argv );
VALUE rabbit_array_at( VALUE self, VALUE pos );
VALUE rabbit_array_len( VALUE self );
VALUE rabbit_array_each( VALUE self );
VALUE rabbit_array_each_with_index( VALUE self );
VALUE rabbit_array_set( VALUE self, VALUE slot_num, VALUE operand );
VALUE rabbit_array_push( VALUE self, int argc, VALUE *argv );
VALUE rabbit_array_pop( VALUE self );
VALUE rabbit_array_shift( VALUE self );
VALUE rabbit_array_clear( VALUE self );
VALUE rabbit_array_to_string( VALUE self );

// Fixnum class methods
VALUE rabbit_fixnum_eq( VALUE self, VALUE n );
VALUE rabbit_fixnum_ne( VALUE self, VALUE n );
VALUE rabbit_fixnum_gt( VALUE self, VALUE n );
VALUE rabbit_fixnum_ge( VALUE self, VALUE n );
VALUE rabbit_fixnum_lt( VALUE self, VALUE n );
VALUE rabbit_fixnum_le( VALUE self, VALUE n );
VALUE rabbit_fixnum_add( VALUE self, VALUE n );
VALUE rabbit_fixnum_sub( VALUE self, VALUE n );
VALUE rabbit_fixnum_mul( VALUE self, VALUE n );
VALUE rabbit_fixnum_div( VALUE self, VALUE n );
VALUE rabbit_fixnum_mod( VALUE self, VALUE n );
VALUE rabbit_fixnum_right_shift( VALUE self, VALUE n );
VALUE rabbit_fixnum_left_shift( VALUE self, VALUE n );
VALUE rabbit_fixnum_uminus( VALUE self );
VALUE rabbit_fixnum_putchar( VALUE self );
VALUE rabbit_fixnum_times( VALUE self );
VALUE rabbit_fixnum_to_string( VALUE self, int argc, VALUE argv[] );
VALUE rabbit_fixnum_random( VALUE self, VALUE min, VALUE max );

// Global functions
VALUE rabbit_f_exit( VALUE self, int argc, VALUE *argv );
VALUE rabbit_f_print( VALUE self, int argc, VALUE *argv );
VALUE rabbit_f_puts( VALUE self, int argc, VALUE *argv );
VALUE rabbit_f_module_load( VALUE self, VALUE module_name );
VALUE rabbit_f_eval( VALUE self, VALUE str );
VALUE rabbit_f_get_version( VALUE self );
VALUE rabbit_f_block_given_p( VALUE self );


#if defined(__cplusplus)
}  // extern "C" {
#endif
#endif // RABBIT_H_

