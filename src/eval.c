#include <ctype.h>
#include <setjmp.h>
#include <stdarg.h>
#include <dlfcn.h>
#include "st.h"
#include "rabbit.h"
#include "node.h"
#include "version.h"

// グローバルスコープ
VALUE rabbit_scope                   = R_NULL;

// Kernel
VALUE rabbit_class                   = R_NULL;

// カレントスコープ
VALUE rabbit_current_scope           = R_NULL;

// カレントノード
NODE  rabbit_current_node            = (NODE)NULL;

// return文の戻り値
VALUE rabbit_return_expression_value = R_NULL;

VALUE rabbit_last_value              = R_NULL;

VALUE eval_expression( VALUE scope, NODE expression );
int parser_get_line();

// ----------------------------------------------------------------------------
// エラーメッセージの表示
// ----------------------------------------------------------------------------
void rabbit_raise_f( const char* head, const char* c_file, int c_line, const char *fmt, ...)
{
	va_list args;

	va_start( args, fmt );

	fprintf( stderr, "%s\n", head  );
	fprintf( stderr, "  C-line: %s:%d\n", c_file, c_line );
	if( rabbit_current_node != (NODE)NULL ) {
		fprintf( stderr, "  line: %ld\n",
			NODE2BASIC(rabbit_current_node)->line );
	}
	fprintf( stderr, "  message: " );
	vfprintf( stderr, fmt, args );
	fprintf( stderr, "\n" );

	va_end( args );
	fflush(stderr);
	abort();
}


#define RABBIT_STACK_MAX   100

// ----------------------------------------------------------------------------
// returnとかbreak用のjmp_bufスタック
// ----------------------------------------------------------------------------
#define TAG_RETURN		0x1
#define TAG_BREAK		0x2
#define TAG_NEXT		0x3

struct rabbit_jmp_buf_stack {
	void* jmp_stack[ RABBIT_STACK_MAX ];
	int stack_ptr;
};

struct rabbit_jmp_buf_stack *Stack_Loop_Jmp;
struct rabbit_jmp_buf_stack *Stack_Method_Jmp;

void rabbit_jmp_buf_stack_init( struct rabbit_jmp_buf_stack **stack )
{
	*stack = malloc( sizeof( struct rabbit_jmp_buf_stack ) );
	(*stack)->stack_ptr = 0;
}

int rabbit_jmp_buf_stack_ptr( struct rabbit_jmp_buf_stack *stack )
{
	return stack->stack_ptr;
}

int rabbit_jmp_buf_stack_push( struct rabbit_jmp_buf_stack *stack, jmp_buf env )
{
	if( stack->stack_ptr >= RABBIT_STACK_MAX ) {
		return RABBIT_STACK_FULL;
	}
	stack->jmp_stack[ stack->stack_ptr ] = env;
	stack->stack_ptr++;

	return RABBIT_STACK_OK;
}

int rabbit_jmp_buf_stack_pop( struct rabbit_jmp_buf_stack *stack, void** env )
{
	if( stack->stack_ptr == 0 ) {
		return RABBIT_STACK_END;
	}
	stack->stack_ptr--;

	if( env != NULL ) {
		*env = stack->jmp_stack[ stack->stack_ptr ];
	}
	return RABBIT_STACK_OK;
}

// ----------------------------------------------------------------------------
// ブロック付きメソッド呼び出し用のスタック
// ----------------------------------------------------------------------------
NODE rabbit_iterator_stack[ RABBIT_STACK_MAX ];
int rabbit_iterator_stack_pos = 0;

int rabbit_iterator_stack_push( NODE block_nd )
{
	if( rabbit_iterator_stack_pos >= RABBIT_STACK_MAX ) {
		return RABBIT_STACK_FULL;
	}
	rabbit_iterator_stack[ rabbit_iterator_stack_pos ] = block_nd;
	rabbit_iterator_stack_pos++;
	
	return RABBIT_STACK_OK;
}

int rabbit_iterator_stack_pop( NODE *block_nd )
{
	if( rabbit_iterator_stack_pos == 0 ) {
		return RABBIT_STACK_END;
	}
	rabbit_iterator_stack_pos--;
	
	if( block_nd != (NODE)NULL ) {
		*block_nd =  rabbit_iterator_stack[ rabbit_iterator_stack_pos ];
	}
	return RABBIT_STACK_OK;
}


// ----------------------------------------------------------------------------
// Cで書かれたメソッドを呼ぶ関数
// ----------------------------------------------------------------------------
VALUE call_c_func( VALUE klass, VALUE object, VALUE method, int argc, VALUE *argv )
{
	rabbit_cfunc cfunc = R_METHOD(method)->u.cfunc_ptr;

	assert( method );

	if( R_METHOD(method)->arg_len >= 0 && R_METHOD(method)->arg_len != argc ) {
		// 引数の数が合わない
		rabbit_raise( "wrong # of arguments(%d for %d)",
			R_METHOD(method)->arg_len, argc );
	}

	switch( R_METHOD(method)->arg_len )
	{
	case -1 :
		return cfunc( object, argc, argv );
	case  0 :
		return cfunc( object );
	case  1 :
		return cfunc( object, argv[0] );
	case  2 :
		return cfunc( object, argv[0], argv[1] );
	case  3 :
		return cfunc( object, argv[0], argv[1], argv[2] );
	case  4 :
		return cfunc( object, argv[0], argv[1], argv[2], argv[3] );
	case  5 :
		return cfunc( object, argv[0], argv[1], argv[2], argv[3], argv[4] );
	case  6 :
		return cfunc( object, argv[0], argv[1], argv[2], argv[3], argv[4], argv[5] );
	case  7 :
		return cfunc( object, argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6] );
	case  8 :
		return cfunc( object, argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7] );
	case  9 :
		return cfunc( object, argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8] );
	default :
		rabbit_raise2( "arguments too long" );
	};

	abort();
}

// ----------------------------------------------------------------------------
// scope                : スコープ
// self                 : オブジェクト
// func_node            : 関数本体が入っているノード
// argc                 : 引数の数
// argv                 : 引数
// parent_scope         : 1個上のスコープ。yield呼び出しの時に必要
// ----------------------------------------------------------------------------
VALUE call_rabbit_func( VALUE klass, VALUE self, NODE func_node, int argc, VALUE *argv, VALUE parent_scope )
{
	volatile VALUE result = R_NULL;
	volatile VALUE local_scope = R_NULL;
	jmp_buf jmp_env;

	assert( func_node );
	
	if( NODE2FUNC(func_node)->block == (NODE)NULL ) {
		return R_NULL;
	}

	if( rabbit_jmp_buf_stack_push( Stack_Method_Jmp, jmp_env ) != RABBIT_STACK_OK ) {
		rabbit_raise2( "method jmp stack overflow" );
	}

	// 関数内は完全なローカルスコープということで。
	local_scope = scope_new( parent_scope ? parent_scope : 0 );

	scope_class_set( local_scope, klass );
	{
		// 渡された引数を分解する
		NODE  param_next = NODE2FUNC(func_node)->params;
		int is_return_end = 0;
		int i = 0;
		
		while( param_next != (NODE)NULL ) {
			if( i >= argc ) {
				// 引数の数が合わない
				rabbit_raise2( "wrong method of arguments" );
			}
			// 引数をパラメータにセットする
			scope_set_value( local_scope, NODE2PARAMS(param_next)->symbol, argv[i] );
			param_next = NODE2PARAMS(param_next)->next;
			++i;
		}
		
		// selfをセットする
//		scope_set_value( local_scope, rabbit_name2tag("self"), self );
		scope_set_value( local_scope, rabbit_tag_self, self );

//		NODE next_expr = NODE2BLOCK( NODE2FUNC(func_node)->block )->list;
		NODE next_expr = NODE2FUNC(func_node)->block;
		
		while( next_expr != (NODE)NULL ) {
			switch( setjmp( jmp_env ) ) {
			case 0:
				result = eval_expression( local_scope, NODE2LIST(next_expr)->node );
				break;
			case TAG_RETURN:
				is_return_end = 1;
				result = rabbit_return_expression_value;
				rabbit_return_expression_value = R_NULL;
				break;
			default:
				rabbit_raise2( "変な値がlongjmpで戻ってきました" );
			}
			if( is_return_end != 0 ) {
				break;
			}
			next_expr = NODE2LIST(next_expr)->next;
		}

		if( is_return_end == 0 ) {
			if( rabbit_jmp_buf_stack_pop( Stack_Method_Jmp, NULL ) != RABBIT_STACK_OK ) {
				rabbit_raise2( "faild method jmp stack pop" );
			}
		}
	}
	scope_dispose( local_scope );

	return result;
}

// ----------------------------------------------------------------------------
// メソッド呼び出し
// ----------------------------------------------------------------------------
VALUE eval_method_call( NODE scope, NODE mcall_node )
{
	VALUE result = R_NULL;
	VALUE recv   = R_NULL;
	VALUE klass, method, old_scope;
	int i;
	NODE  arg_node;
	int   argc;
	VALUE *argv; 
	
	assert( mcall_node );

	NODE recv_nd  = NODE2MCALL(mcall_node)->recv;
	TAG method_id = NODE2MCALL(mcall_node)->method;

	if( recv_nd != (NODE)NULL ) {
		recv = eval_expression( scope, recv_nd );
	} else {
		// レシーバ無しの呼び出しだったら、selfを付けてあげる。
		VALUE self;

		if( scope_get_value( scope, rabbit_tag_self, SCOPE_SEARCH_E_ONLY, (VALUE)NULL, &self ) == 0 ) {
			rabbit_raise( "スコープ内に'%s'が無いです", "self" );
		}
		recv = self;
	}

	klass = CLASS_OF(recv);

	if( klass == R_NULL ) {
		rabbit_raise( "まだそのクラスは作っていません(value_type: %d)\n", VALUE_TYPE(recv) );
	}
	
	// パラメータ分解
	// GCで拾うために mallocでは無く、allocaで確保してね。
	arg_node = NODE2MCALL(mcall_node)->arguments;
	argc     = arg_node ? NODE2LIST(arg_node)->len : 0;
	argv     = alloca( sizeof(VALUE)* argc ); 

	for( i=0; i<argc; ++i ) {
		if( NODE_TYPE( NODE2LIST(arg_node)->node ) ==  NODE_E_VALUE ) {
			argv[i] = NODE2VALUE( NODE2LIST(arg_node)->node );
		} else {
			argv[i] = eval_expression( scope, NODE2LIST(arg_node)->node );
		}
		arg_node = NODE2LIST(arg_node)->next;
	}

	// メソッドの取得
	method = class_method_lookup( R_CLASS(klass), method_id );

	if( method == R_NULL || VALUE_TYPE( method ) != TYPE_METHOD ) {
		rabbit_raise( "`%s` method not found..", rabbit_tag2name(method_id) );
	}

	// ブロック付き呼び出しだったら、ブロックをスタックに詰む
	if( NODE2MCALL(mcall_node)->func_block != (NODE)NULL ) {
		if( rabbit_iterator_stack_push( NODE2MCALL(mcall_node)->func_block ) != RABBIT_STACK_OK ) {
			rabbit_raise2( "faild iterator stack push" );
		}
	}

	// 消しちゃダメ！
	old_scope = rabbit_current_scope;
	rabbit_current_scope = scope;
	
	switch( R_METHOD(method)->func_type ) {
	case FUNCTION_E_C:
		result = call_c_func( klass, recv, method, argc, argv );
		break;
	case FUNCTION_E_RABBIT: {
		struct RNodeFunction *func_node = R_METHOD(method)->u.func_node;
		result = call_rabbit_func( klass, recv, (NODE)func_node, argc, argv, 0 );
		break;
	}
	default:
		rabbit_bug2( "undef method" );
	}

	if( NODE2MCALL(mcall_node)->func_block != (NODE)NULL ) {
		// 関数呼び出しが終わったのでブロックをスタックから降ろす
		if( rabbit_iterator_stack_pop( (NODE)NULL ) != RABBIT_STACK_OK ) {
			rabbit_raise2( "iterator stack empty" );
		}
	}
	rabbit_current_scope = old_scope;


	return result;
}

// ----------------------------------------------------------------------------
// yieldを処理する
// ----------------------------------------------------------------------------
//VALUE rabbit_yield( VALUE scope, int argc, VALUE *argv )
VALUE rabbit_yield( VALUE self, int argc, VALUE *argv )
{
	VALUE result = R_NULL;
	VALUE yield_scope = R_NULL;
	NODE  iterator_block;

	// イテレータをスタックから降ろす
	if( rabbit_iterator_stack_pop( &iterator_block ) != RABBIT_STACK_OK ) {
		rabbit_raise2( "faild iterator stack empty" );
	}

	// カレントスコープの上にyield用のスコープ作る
	yield_scope = scope_new( rabbit_current_scope );
	{
		result = call_rabbit_func( CLASS_OF(self), self, iterator_block, argc, argv, yield_scope );
	}
	scope_dispose( yield_scope );
	
	// スタックに詰みなおす
	if( rabbit_iterator_stack_push( iterator_block ) != RABBIT_STACK_OK ) {
		rabbit_raise2( "faild iterator stack push" );
	}
	return result;
}

// ----------------------------------------------------------------------------
// 式を評価する
// ----------------------------------------------------------------------------
VALUE eval_expression( VALUE scope, NODE expression )
{
	VALUE result;

	assert( expression );

	result = R_NULL;

EVAL_START:
	rabbit_current_node = expression;

	switch( NODE_TYPE(expression) )
	{
	case NODE_E_VALUE: {
		// 値
		return NODE2VALUE( expression );
	}
	case NODE_E_SYMBOL: { 
		// シンボル名
		int exists = 0;
		TAG symbol = NODE2SYMBOL(expression)->symbol;

		assert(symbol);
		
		switch( TAG_FLAG(symbol) )
		{
		case SYMBOL_E_LOCAL:
			// ローカル変数を探して
			if( (exists = scope_get_value( scope, symbol, SCOPE_SEARCH_E_CHAIN, NULL, &result )) == 0 ) {
				// 無かったらクラスを探す
				if( (result = rabbit_class_lookup( symbol )) != R_NULL ) {
					exists = 1;
				}
			}
			break;
		case SYMBOL_E_GLOBAL:
			exists = scope_get_value( rabbit_scope, symbol, SCOPE_SEARCH_E_ONLY, NULL, &result );
			break;
		case SYMBOL_E_IVAL: {
			VALUE self;
			if( scope_get_value( scope, rabbit_tag_self, SCOPE_SEARCH_E_ONLY, 0, &self ) == 0 ) {
				rabbit_raise2( "スコープにselfがありませんでした" );
			}
			if( rabbit_get_instance_value( self, symbol, &result ) != R_NULL ) {
				exists = 1;
			}
			break;
		}
		default:
			rabbit_raise2( "シンボルフラグが変です" );
		}

		if( exists == 0 ) {
			rabbit_raise( "var `%s' not found..", rabbit_tag2name(symbol) );
		}
		return result;
	}
	case NODE_E_IF: { // if文
		VALUE condition = eval_expression( scope, NODE2IF(expression)->condition );
		NODE elsif_list = NODE2IF(expression)->elsif_list ;

		result = R_NULL;

		if( IS_TRUE( condition ) ) {
			if( NODE2IF(expression)->then_block != (NODE)NULL ) {
				result = eval_expression( scope, NODE2IF(expression)->then_block );
			}
			return result;
		}

		if( elsif_list != (NODE)NULL ) {
			NODE next = elsif_list;
			NODE elsif;

			while( next != (NODE)NULL ) {
				elsif = NODE2LIST(next)->node;
				condition = eval_expression( scope, NODE2IF(elsif)->condition );

				if( IS_TRUE(condition) ) {
					if( NODE2IF(expression)->then_block != (NODE)NULL ) {
						result = eval_expression( scope, NODE2IF(elsif)->then_block );
					}
					return result;
				}
				next = NODE2LIST(next)->next;
			}
		}

		if( NODE2IF(expression)->else_block != (NODE)NULL ) {
			return eval_expression( scope, NODE2IF(expression)->else_block );
		}
		return R_NULL;
	}
	case NODE_E_TERNARY: {
		// 三項演算子
		VALUE condition = eval_expression( scope, NODE2IF(expression)->condition );

		expression = IS_TRUE( condition ) 
			? NODE2IF(expression)->then_block
			: NODE2IF(expression)->else_block;

		goto EVAL_START;
	}
	case NODE_E_ASSIGN: {
		// 代入
		NODE left_nd =  NODE2ASSIGN(expression)->left_nd;
		
		// 左辺がシンボルじゃない。
		if( NODE_TYPE( left_nd ) != NODE_E_SYMBOL ) {
			rabbit_raise2( "左辺値がシンボルノードではありません。" );
		}

		// 右辺ノードを評価する
		VALUE operand_val = eval_expression( scope, NODE2ASSIGN(expression)->operand );
		TAG symbol  = NODE2SYMBOL(left_nd)->symbol;

		assert(symbol);
		
		switch( TAG_FLAG(symbol) )
		{
		case SYMBOL_E_GLOBAL:
			scope_set_value( rabbit_scope, symbol, operand_val );
			break;
		case SYMBOL_E_IVAL: {
			VALUE self;
			if( scope_get_value( scope, rabbit_tag_self, SCOPE_SEARCH_E_ONLY, 0, &self ) == 0 ) {
				rabbit_bug2( "スコープにselfがありませんでした" );
			}
			rabbit_set_instance_value( self, symbol, operand_val );
			break;
		}
		case SYMBOL_E_LOCAL:
			scope_set_value( scope, symbol, operand_val );
			break;
		default:
			rabbit_bug2( "シンボルフラグが変です" );
		}
		return operand_val;
	}
	case NODE_E_YIELD: { 
		// yield
		VALUE self;
		VALUE *argv;
		int argc;
		int i;
		NODE arg_node;

		// ブロックに送るパラメータ分解
		// パラメータ分解
		// GCで拾うために mallocでは無く、allocaで確保してね。
		arg_node = NODE2YIELD(expression)->arguments;
		argc     = arg_node ? NODE2LIST(arg_node)->len : 0;
		argv     = argc ? alloca( sizeof(VALUE)* argc ): 0; 
		
		for( i=0; i<argc; ++i ) {
			if( NODE_TYPE( NODE2LIST(arg_node)->node ) ==  NODE_E_VALUE ) {
				argv[i] = NODE2VALUE( NODE2LIST(arg_node)->node );
			} else {
				argv[i] = eval_expression( scope, NODE2LIST(arg_node)->node );
			}
			arg_node = NODE2LIST(arg_node)->next;
		}
//		parse_arguments( scope, NODE2YIELD(expression)->arguments, &argc, &argv );

		if( scope_get_value( scope, rabbit_tag_self, SCOPE_SEARCH_E_ONLY, 0, &self ) == 0 ) {
			rabbit_bug2( "スコープにselfがありませんでした" );
		}
		result = rabbit_yield( self, argc, argv );

		if( argc > 0 ) {
			//free( argv );
		}
		return result;
	}
	case NODE_E_BREAK: {
		// break文
		int is_break = 1;
		NODE condition_nd = NODE2BREAK(expression)->condition;

		if( condition_nd != 0 ) {
			result = eval_expression( scope, condition_nd );
			is_break = IS_TRUE(result) ? 1 : 0;
		}
		if( is_break == 1 ) {
			void* jmp_env;

			if( rabbit_jmp_buf_stack_pop( Stack_Loop_Jmp, &jmp_env ) != RABBIT_STACK_OK ) {
				rabbit_raise2( "jmp stack pop || die" );
			}
			longjmp( jmp_env, TAG_BREAK );
		}
		abort();
	}
	case NODE_E_WHILE: {
		// whileループ
		jmp_buf env;
		int is_break_end = 0;
		NODE while_expr = expression;

		if( rabbit_jmp_buf_stack_push( Stack_Loop_Jmp, env ) != RABBIT_STACK_OK ) {
			rabbit_raise2( "jmp stack overflow" );
		}

		while(1) {
			VALUE condition = eval_expression( scope, NODE2WHILE(while_expr)->condition );

			if( IS_FALSE(condition)  ) {
				break;
			}
			if( NODE2WHILE(while_expr)->block != (NODE)NULL ) {
				switch( setjmp( env ) )
				{
				case 0:
					result = eval_expression( scope, NODE2WHILE(while_expr)->block );
					break;
				case TAG_BREAK:
					// break命令なのでループを抜ける
					is_break_end = 1;
					break;
				default:
					rabbit_raise2( "変な値がlong jmpで戻ってきた(while)" );
				}
			}
			if( is_break_end != 0 ) {
				break;
			}
		}
		
		if( is_break_end == 0 ) {
			if( rabbit_jmp_buf_stack_pop( Stack_Loop_Jmp, NULL ) != RABBIT_STACK_OK ) {
				rabbit_raise2( "method jmp stack pop || die" );
			}
		}
		return result;
	}
	case NODE_E_METHOD_CALL:
		// メソッド呼び出し
		return eval_method_call( scope, expression );

	case NODE_E_RETURN: {
		// return文
		void *env;

		if( NODE2RETURN(expression)->expr != R_NULL ) {
			rabbit_return_expression_value = eval_expression( scope, NODE2RETURN(expression)->expr );
		} else {
			rabbit_return_expression_value = R_NULL;
		}
		if( rabbit_jmp_buf_stack_pop( Stack_Method_Jmp, &env ) != RABBIT_STACK_OK ) {
			rabbit_raise2( "method jmp stack pop || die" );
		}
		longjmp( env, TAG_RETURN );
		abort();
	}
	case NODE_E_ARRAY_LITERAL: {
		// 配列リテラル
		NODE list = NODE2LITERAL(expression)->node;

		// インスタンス作る
		result = rabbit_array_new();

		// リテラルの中身を全部pushする
		while( list != (NODE)NULL ) {
			VALUE val[1] = {
				eval_expression( scope, NODE2LIST(list)->node )
			};
			rabbit_array_push( result, 1, val );
			list =  NODE2LIST(list)->next;
		}
		return result;
	}
	case NODE_E_LIST: {
		NODE list;
		list = expression;
		while( list != (NODE)NULL ) {
			result = eval_expression( scope, NODE2LIST(list)->node );
			list = NODE2LIST(list)->next;
		}
		return result;
	}
	case NODE_E_CLASS_DEFIN: {
		 // クラスの定義
		TAG  symbol = NODE2CLASS(expression)->symbol;
		VALUE klass;
		VALUE super = R_NULL;

		if( NODE2CLASS(expression)->super != 0 ) {
			if( (super = rabbit_class_lookup( NODE2CLASS(expression)->super )) == R_NULL ) {
				rabbit_raise( "error - super klass '%p' notfound.", (void*)(NODE2CLASS(expression)->super) );
			}
		}
		
		klass = rabbit_define_class( (char*)(NODE2CLASS(expression)->identifier), super );

		// ブロックが空じゃなかったら評価する
		if( NODE2CLASS(expression)->block != (NODE)NULL ) {
			// クラススコープ作る
			VALUE class_scope = scope_new( scope );
			{
				scope_class_set( class_scope, klass );
				scope_set_value( class_scope, rabbit_tag_self, klass );
				result = eval_expression( class_scope, NODE2CLASS(expression)->block );
			}
			scope_dispose( class_scope );
		}
		scope_set_value( scope, symbol, klass );

		return R_NULL;
	}
	case NODE_E_METHOD_DEFIN: {
		// メソッドの定義
		VALUE scope_class = scope_class_get( scope );
		const char* identifier = NODE2METHOD(expression)->identifier;

		NODE params = NODE2METHOD(expression)->params;
		NODE block  = NODE2METHOD(expression)->block;
		NODE func_node = node_function_new( params, block );

		rabbit_define_method_node( scope_class, (char*)identifier, (struct RNodeFunction *)(func_node) );
		
		return R_NULL;
	}

	case NODE_E_LOGNOT:
		// 否定演算子 !block_given? とか。
		return IS_TRUE( eval_expression( scope, NODE2LOGNOT(expression)->expr ) ) ? R_FALSE : R_TRUE;
	
	default:
		rabbit_bug( "eval expression non support type(%s) %d\n", NODE_TYPE_NAME(NODE_TYPE(expression)), NODE_TYPE(expression) );
	}

	return result;
}


VALUE eval_rabbit( NODE rabbit_tree )
{
	VALUE result;
	NODE list;

	result = R_NULL;
	
	if( rabbit_tree != (NODE)NULL ) {
		if( NODE_TYPE(rabbit_tree) != NODE_E_LIST ) {
			fprintf( stderr, "ツリーの構造がよくわかりません!\n" );
			abort();
		}

		//list = NODE2BLOCK(rabbit_tree)->list;
		list = rabbit_tree;

		while( list != (NODE)NULL ) {
			result = eval_expression( rabbit_scope, NODE2LIST(list)->node );
			list = NODE2LIST(list)->next;
		}
	}
	rabbit_last_value = result;
	
	return result;
}

VALUE rabbit_f_block_given_p( VALUE self )
{
	NODE block_nd;
	if( rabbit_iterator_stack_pop( &block_nd ) == RABBIT_STACK_OK ) {
		rabbit_iterator_stack_push( block_nd );
		return R_TRUE;
	}
	return R_FALSE;
}

VALUE rabbit_f_module_load( VALUE self, VALUE module_name )
{
	void *handle;
	void (*func)();
	char *error, *ext_name;
	char *func_ext = "_Init";
	char *func_name;
	int  len;

	if( VALUE_TYPE(module_name) != TYPE_STRING ) {
		rabbit_raise2("文字列を渡してくだしあ");
	}

	len = strlen(R_STRING(module_name)->ptr) + strlen(func_ext) + 1;
	func_name = rabbit_calloc( sizeof(char), len );
	
	strncpy( func_name, R_STRING(module_name)->ptr, R_STRING(module_name)->len );
	strncat( func_name, func_ext, strlen(func_ext) );
	*func_name = toupper( *func_name );
	
	ext_name = rabbit_calloc( sizeof(char), R_STRING(module_name)->len + 4 );
	
	strncpy( ext_name, R_STRING(module_name)->ptr, R_STRING(module_name)->len );
	strncat( ext_name, ".so", 3 );

	if( !(handle = dlopen( ext_name, RTLD_LAZY | RTLD_GLOBAL )) ) {
		rabbit_raise( "%s open error", ext_name );
	}

	func = dlsym( handle, func_name );

	if ((error = dlerror()) != NULL)  {
		rabbit_raise2( error );
	}
	func();
	
	free( ext_name );
	free( func_name );
	
	return R_NULL;
}

void rabbit_gloval_variables( TAG id, VALUE *value )
{
	rabbit_gc_global( value );
	scope_set_value( rabbit_scope, id, *value );
}

static int symbol_each__( char* key, TAG val, VALUE arr )
{
	VALUE args[1] = { rabbit_string_new( strlen(key), key ) };
	rabbit_array_push( arr, 1, args );

	return ST_CONTINUE;
}

VALUE rabbit_f_symbols( VALUE self )
{
	extern struct st_table* rabbit_symbol_tbl;
	VALUE result = rabbit_array_new();
	
	if( rabbit_f_block_given_p( R_NULL ) == R_FALSE ) {
//		return R_NULL;
	}
	st_foreach( rabbit_symbol_tbl, symbol_each__, result );

	return result;
}

void Rabbit_Setup()
{
	int temp;
	
	Init_Stack((void*)&temp);

	Heap_Setup();
	Symbol_Setup();
	Object_Setup();
	Array_Setup();
	Scope_Setup();
	Numeric_Setup();
	String_Setup();
	Gc_Setup();
	
	rabbit_jmp_buf_stack_init( &Stack_Loop_Jmp );
	rabbit_jmp_buf_stack_init( &Stack_Method_Jmp );

	rabbit_tag_initialize = rabbit_name2tag("initialize");
	rabbit_tag_self       = rabbit_name2tag("self");

	rabbit_class = C_Kernel;
	rabbit_scope = scope_new( R_NULL );

	scope_class_set( rabbit_scope, C_Kernel );

	rabbit_gloval_variables( rabbit_name2tag("TRUE"),  &C_TrueClass );
	rabbit_gloval_variables( rabbit_name2tag("FALSE"), &C_FalseClass );
	
	rabbit_define_global_function( "block_given?", rabbit_f_block_given_p, 0 );
	rabbit_define_global_function( "eval", rabbit_f_eval, 1 );
	rabbit_define_global_function( "load", rabbit_f_module_load, 1 );
	rabbit_define_global_function( "symbols", rabbit_f_symbols, 0 );
	
	scope_set_value( rabbit_scope, rabbit_tag_self, C_Kernel );
}

int yyerror( char const *str )
{
	extern char* yytext;
	
	fprintf( stderr, "parser error line %d - near %s \n", parser_get_line(), yytext );
	return 0;
}

void rabbit_usage_exit()
{
	fprintf( stderr, "usage: rabbit.exe [option..] file [args..]\n" );
	exit(1);
}

int rabbit_run( int argc, char* argv[] )
{
	extern FILE *yyin;
	extern int yydebug;
	extern int yyparse(void);
	
	int i, show_version = 0;
	char* file = NULL;
	char* e_string = 0;
	VALUE args;

	if( argc < 2 ) {
		rabbit_usage_exit();
	}

	Rabbit_Setup();

	rabbit_current_scope = rabbit_scope;

	args = rabbit_array_new();
	scope_set_value( rabbit_scope, rabbit_name2tag("ARGV"), args );

	for( i=1; i<argc; ++i ) {
		if( file == NULL && argv[i][0] == '-' ) {
			if( strlen( argv[i] ) == 1 ) {
				rabbit_raise2( "unknown option `-`" );
			}
			if( strcmp( argv[i], "-yydebug" ) == 0 ) {
				yydebug = 1;
				continue;
			}
			switch(argv[i][1])
			{
			case 'v': {
				char *version = rabbit_version_dup();
				printf( "%s\n", version );
				free( version );
				show_version = 1;
				break;
			}
			case 'e' : {
				if( i == argc-1 ) {
					fprintf( stderr, "rabbit -- no code specified for -e\n" );
					abort();
				}
				e_string = strdup( argv[ ++i ] );
				break;
			}
			default :
				rabbit_raise( "unknown option `%s`", argv[i] );
			}
			continue;
		}
		if( file == NULL ) {
			file = argv[i];
			continue;
		}
		VALUE v_argv[1] = { rabbit_string_new( strlen( argv[i] ), argv[i] ) };
		rabbit_array_push( args, 1, v_argv );
	}

	if( file == NULL && e_string == 0 ) {
		if( show_version == 1 ) {
			return 0;
		}
		rabbit_usage_exit();
	}
	if( e_string == 0 ) {
		if( (yyin = fopen( file, "r" )) == NULL ) {
			rabbit_raise( "No such file or directory -- %s", file );
		}
		Init_Lexer(yyin);
		
		if( yyparse() ) {
			fprintf( stderr, "[(;_;)]Rabbit parser error\n");
		}
		if( yyin != NULL ) {
			fclose( yyin );
		}
	} else {
		rabbit_f_eval( R_NULL, rabbit_string_new( strlen(e_string), e_string ) );
		free( e_string );
	}

	scope_dispose( rabbit_scope );

	return 0;
}

