#include "rabbit.h"
#include "node.h"
#include "st.h"

#define debug_write(fmt,...)  fprintf( stderr, fmt "\n", __VA_ARGS__ )
#define RABBIT_SCOPE_STACK_SIZE 100

static VALUE rabbit_scope_stack[ RABBIT_SCOPE_STACK_SIZE ];
static int rabbit_scope_stack_pos = 0;

int scope_stack_pop( VALUE *scope )
{
	if( rabbit_scope_stack_pos == 0 ) {
		return RABBIT_STACK_END;
	}
	rabbit_scope_stack_pos--;
	*scope = rabbit_scope_stack[ rabbit_scope_stack_pos ];
	
	return RABBIT_STACK_OK;
}

int scope_stack_push( VALUE scope )
{
	if( rabbit_scope_stack_pos >= RABBIT_SCOPE_STACK_SIZE ) {
		return RABBIT_STACK_FULL;
	}
	rabbit_scope_stack[ rabbit_scope_stack_pos ] = scope;
	rabbit_scope_stack_pos++;
	
	return RABBIT_STACK_OK;
}

void active_scope_foreach( void (*callback)(VALUE) )
{
	int i;
	
	for( i=rabbit_scope_stack_pos; i<RABBIT_SCOPE_STACK_SIZE; ++i ) {
		callback( rabbit_scope_stack[i] );
	}
}

VALUE scope_create__()
{
	struct RScope *scope;
	
	scope = (struct RScope *)rabbit_malloc( sizeof(struct RScope) );

	RABBIT_OBJECT_SETUP( scope, 0, TYPE_SCOPE );
	{
		scope->var_tbl = st_init_numtable();
		scope->parent  = 0;
		scope->klass   = 0;
		scope->is_used = 0;
	}
	
	return (VALUE)scope;
}

static int scope_values_each_callback__( TAG tag, VALUE val, void (*call_back)(TAG,VALUE) )
{
	call_back( tag, val );

	return ST_CONTINUE;
}

void scope_values_each( VALUE scope, void (*callback)(TAG,VALUE) )
{
	st_foreach( R_SCOPE(scope)->var_tbl, scope_values_each_callback__, (st_data_t)callback );
}

void scope_clear__( VALUE scope )
{
	assert( scope );
	
	R_SCOPE(scope)->parent = 0;
	R_SCOPE(scope)->klass  = 0;
	
	// ハッシュテーブルの内容をクリアする
//	st_foreach( R_SCOPE(scope)->var_tbl, scope_values_delete_callback__, (st_data_t)NULL );
//	st_cleanup_safe( R_SCOPE(scope)->var_tbl, R_UNDEF );

	st_free_table( R_SCOPE(scope)->var_tbl );
	R_SCOPE(scope)->var_tbl = st_init_numtable();
}

int scope_set_value( VALUE scope, TAG tag, VALUE value )
{
	assert( scope );
	assert( tag );
	
	R_SCOPE(scope)->is_used = 1;

	if( VALUE_TYPE(scope) != TYPE_SCOPE ) {
		fprintf( stderr, "type error - scope_set_value scope\n" );
		abort();
	}
	
	int result = st_insert( R_SCOPE(scope)->var_tbl, tag, (st_data_t)value );

	return result;
}

int scope_get_value(
	VALUE scope, TAG tag, enum scope_search_type search_type,
	VALUE *hit_scope, VALUE *hit_value
) {
	VALUE current_scope;
	VALUE temp_hit_scope = R_NULL;
	VALUE value = R_NULL;

	assert( scope );
	assert( tag );
	
	if( VALUE_TYPE(scope) != TYPE_SCOPE ) {
		fprintf( stderr, "type error - scope_set_value scope\n" );
		abort();
	}
	*hit_value = R_NULL;

	current_scope = scope;

	// スコープチェインをスキャンする
	while( current_scope != 0 ) {
		
		if( st_lookup( R_SCOPE(current_scope)->var_tbl, tag, &value ) != 0 ) {
			temp_hit_scope = current_scope;
			*hit_value = value;
			break;
		} else if( search_type == SCOPE_SEARCH_E_ONLY ) {
			// ローカル変数だけのスキャンだったら抜ける
			break;
		}
		current_scope =  R_SCOPE(current_scope)->parent;
	}
	
	if( hit_scope != R_NULL ) {
		*hit_scope = temp_hit_scope;
	}
	return temp_hit_scope != R_NULL ? 1 : 0;
}

VALUE scope_new( VALUE parent_scope )
{
	VALUE scope;
	
	if( scope_stack_pop( &scope ) != RABBIT_STACK_OK ) {
		fprintf( stderr, "[(;_;)]rabbit scope stack empty - %s:%d\n", __FILE__, __LINE__ );
		abort();
	}
	
	if( R_SCOPE(scope)->is_used != 0 ) {
	//	STDERR_( "stack .. %d\n", rabbit_scope_stack_pos );
		scope_clear__( scope );
	}
	R_SCOPE(scope)->parent = parent_scope;
	
	return scope;
}

void scope_dispose( VALUE scope )
{
	if( VALUE_TYPE(scope) != TYPE_SCOPE ) {
		fprintf( stderr, "type error - scope_dispose scope\n" );
		abort();
	}
	if( scope_stack_push( scope ) != RABBIT_STACK_OK ) {
		fprintf( stderr, "[m(>_<)m bug] rabbit scope stack full - %s:%d\n", __FILE__, __LINE__ );
		abort();
	}
}

void scope_class_set( VALUE scope, VALUE klass )
{
	assert( scope );
	assert( klass );

	if( VALUE_TYPE(scope) != TYPE_SCOPE ) {
		fprintf( stderr, "type error - scope_class_set scope\n" );
		abort();
	}
	if( VALUE_TYPE(klass) != TYPE_CLASS ) {
		fprintf( stderr, "type error - scope_class_set klass\n" );
		abort();
	}

	R_SCOPE(scope)->klass = klass;
}

VALUE scope_class_get( VALUE scope )
{
	VALUE current_scope;
	VALUE result = R_NULL;
	
	assert( scope );

	if( VALUE_TYPE(scope) != TYPE_SCOPE ) {
		fprintf( stderr, "type error - scope_class_get scope\n" );
		abort();
	}

	current_scope = scope;

	while( current_scope != 0 ) {
		result = R_SCOPE(current_scope)->klass;
		if( result != R_NULL ) {
			break;
		}
		current_scope =  R_SCOPE(current_scope)->parent;
	}
	
	if( result == R_NULL ) {
		fprintf( stderr, "scope_class_get klass lost\n" );
		abort();
	}
	return result;
}

void Scope_Setup()
{
	int i;
	
	for( i=0; i < RABBIT_SCOPE_STACK_SIZE; ++i ) {
		rabbit_scope_stack[i] = scope_create__();
		rabbit_scope_stack_pos++;
	}
}

