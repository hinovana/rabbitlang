#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "rabbit.h"
#include "node.h"

int parser_get_line();

#define node_set_basic(nd_,nd_type_,c_type_) do {			\
	struct RNode basic;										\
	basic.type = nd_type_;									\
	basic.line = parser_get_line();							\
	((c_type_ *)(nd_))->basic = basic;						\
} while(0)


TAG rabbit_name2tag( const char* symbol );

NODE node_new( enum rabbit_node_type type )
{
	struct RNode *node;
	
	node = (struct RNode*)malloc( sizeof(struct RNode) );
	{
		node->type = type;
	}
	return (NODE)node;
}

//NODE node_function_new( NODE params_node, NODE block_node )

NODE node_yield_new( NODE list_nd )
{
	struct RNodeYield *yield_node;
		
	yield_node = (struct RNodeYield*)malloc( sizeof(struct RNodeYield) );
	{
		yield_node->arguments = list_nd;

		node_set_basic( (NODE)yield_node, NODE_E_YIELD, struct RNodeYield );
	}
	return (NODE)yield_node;
}

NODE node_logical_not_new( NODE expr )
{
	struct RNodeLognot *lognot_nd;
	
	lognot_nd = (struct RNodeLognot*)malloc( sizeof(struct RNodeLognot) );
	{
		lognot_nd->expr = expr;

		node_set_basic( (NODE)lognot_nd, NODE_E_LOGNOT, struct RNodeLognot );
	}
	return (NODE)lognot_nd;
}

NODE node_return_new( NODE expr )
{
	struct RNodeReturn *return_node;
	
	return_node = (struct RNodeReturn*)malloc( sizeof(struct RNodeReturn) );
	{
		return_node->expr = expr;

		node_set_basic( (NODE)return_node, NODE_E_RETURN, struct RNodeReturn );
	}
	return (NODE)return_node;
}

NODE node_break_new( NODE condition )
{
	struct RNodeBreak *break_node;
	
	break_node = (struct RNodeBreak*)malloc( sizeof(struct RNodeBreak) );
	{
		break_node->condition = condition;

		node_set_basic( (NODE)break_node, NODE_E_BREAK, struct RNodeBreak );
	}
	return (NODE)break_node;
}

NODE node_array_literal_new( NODE arguments )
{
	struct RNodeLiteral *literal_node;

	literal_node = (struct RNodeLiteral*)malloc( sizeof(struct RNodeLiteral) );
	{
		literal_node->node = arguments;
		
		node_set_basic( (NODE)literal_node, NODE_E_ARRAY_LITERAL, struct RNodeLiteral );
	}
	return (NODE)literal_node;
}

NODE node_call_new( NODE recv_nd, const char* method_name, NODE arguments_nd, NODE func_block_nd )
{
	struct RNodeMcall *mcall_node;
	
	mcall_node = (struct RNodeMcall*)malloc( sizeof(struct RNodeMcall) );
	{
		mcall_node->recv       = recv_nd;
		mcall_node->arguments  = arguments_nd;
		mcall_node->func_block = func_block_nd;
		mcall_node->method     = rabbit_name2tag(method_name);

		node_set_basic( (NODE)mcall_node, NODE_E_METHOD_CALL, struct RNodeMcall );
	}
	return (NODE)mcall_node;
}

NODE node_recv_new( NODE node, const char *method_name )
{
	struct RNodeRecv *recv;
	
	assert( method_name );
	
	recv = (struct RNodeRecv*)malloc( sizeof(struct RNodeRecv) );
	{
		recv->node = node;
		recv->method_name = method_name;
		
		node_set_basic( (NODE)recv, NODE_E_RECV, struct RNodeRecv );
	}
	return (NODE)recv;
}

NODE node_method_call_block_set( NODE mcall_node, NODE params_nd, NODE list_nd )
{
	assert( mcall_node );
	
	if( NODE_TYPE(mcall_node) != NODE_E_METHOD_CALL ) {
		fprintf( stderr, "parser error - %s:%d \n", __FILE__, __LINE__ );
		abort();
	}
	NODE2MCALL(mcall_node)->func_block = node_function_new( params_nd, list_nd );

	return mcall_node;
}


NODE node_list_new( NODE first_node )
{
	struct RNodeList *list_node;
	
	list_node = (struct RNodeList*)malloc( sizeof(struct RNodeList) );
	{
		list_node->node = first_node;
		list_node->next = (NODE)NULL;
		list_node->len  = first_node ? 1 : 0;
		
		node_set_basic( (NODE)list_node, NODE_E_LIST, struct RNodeList );
	}
	return (NODE)list_node;
}

NODE node_list_add( NODE list_node, NODE add_node )
{
	NODE new_list;
	NODE last;
	
	assert( list_node );
	assert( add_node );
	
	if( list_node == 0 || NODE_TYPE(list_node) != NODE_E_LIST ) {
		fprintf( stderr, "type error - node type %d\n", NODE_TYPE(list_node) );
		abort();
	}

	NODE2LIST(list_node)->len++;

	if( NODE2LIST(list_node)->node == (NODE)NULL ) {
		NODE2LIST(list_node)->node = add_node;
		return list_node;
	}

	new_list = node_list_new( add_node );
	last = list_node;

	while(1) {
		if( NODE2LIST(last)->next == 0 ) {
			break;
		}
		last = NODE2LIST(last)->next;
	}
	NODE2LIST(last)->next = new_list;
	
	return list_node;
}

NODE node_class_defin_new( char* symbol, NODE block, char *super )
{
	struct RNodeClass *class_node;
	
	assert( symbol );

	class_node = (struct RNodeClass*)malloc( sizeof(struct RNodeClass) );
	{
		class_node->symbol = rabbit_name2tag( symbol );
		class_node->block  = block;
		class_node->super  = super != (NODE)NULL ? rabbit_name2tag( super ) : (NODE)NULL;

		class_node->identifier = symbol;

		node_set_basic( (NODE)class_node, NODE_E_CLASS_DEFIN, struct RNodeClass );
	}
	return (NODE)class_node;
}

NODE node_method_defin_new( char* symbol, NODE params, NODE block )
{
	struct RNodeMethod *method_node;

	assert( symbol );
	
	method_node = (struct RNodeMethod*)malloc( sizeof(struct RNodeMethod) );
	{
		method_node->symbol = rabbit_name2tag( symbol );
		method_node->params = params;
		method_node->block  = block;
		method_node->identifier = symbol;

		node_set_basic( (NODE)method_node, NODE_E_METHOD_DEFIN, struct RNodeMethod );
		
	}
	return (NODE)method_node;
}

NODE node_intvalue_new( int fixnum )
{
	struct RNodeValue *int_node;
	VALUE value;
	
	value = INT2FIX( fixnum );
	
	int_node = (struct RNodeValue*)malloc( sizeof(struct RNodeValue) );
	{
		int_node->value = value;

		node_set_basic( (NODE)int_node, NODE_E_VALUE, struct RNodeValue );
	}
	return (NODE)int_node;
}

NODE node_stringvalue_new( char* strval )
{
	struct RNodeValue *value_node;
	VALUE value;
	
	value = rabbit_string_new( strlen(strval), strval );

	rabbit_gc_literal( value );
	
	value_node = (struct RNodeValue*)malloc( sizeof(struct RNodeValue) );
	{
		value_node->value = value;

		node_set_basic( (NODE)value_node, NODE_E_VALUE, struct RNodeValue );
	}
	return (NODE)value_node;
}

NODE node_truevalue_new()
{
	struct RNodeValue *true_node;
	VALUE value;
	
	value = R_TRUE;

	true_node = (struct RNodeValue*)malloc( sizeof( struct RNodeValue ) );
	{
		true_node->value = value;
		
		node_set_basic( (NODE)true_node, NODE_E_VALUE, struct RNodeValue );
	}
	assert( true_node );
	
	return (NODE)true_node;
}

NODE node_falsevalue_new()
{
	struct RNodeValue *false_node;
	VALUE value;
	
	value = R_FALSE;

	false_node = (struct RNodeValue*)malloc( sizeof( struct RNodeValue ) );
	{
		false_node->value = value;

		node_set_basic( (NODE)false_node, NODE_E_VALUE, struct RNodeValue );
	}
	assert( false_node );

	return (NODE)false_node;
}

NODE node_ternary_operation_new( NODE condition, NODE then_node, NODE else_node )
{
	NODE result;
	
	result = node_if_new( condition );
	result = node_if_then_block_set( result, then_node );
	result = node_if_else_block_set( result, else_node );
	
	node_set_basic( (NODE)result, NODE_E_TERNARY, struct RNodeIf );
	
	return result;
}

NODE node_if_new( NODE condition )
{
	struct RNodeIf *if_node;
	
	assert( condition );
	
	if_node = (struct RNodeIf*)malloc( sizeof(struct RNodeIf) );
	{
		if_node->condition = condition;
		if_node->then_block = (NODE)NULL;
		if_node->else_block = (NODE)NULL;
		if_node->elsif_list = (NODE)NULL;
		
		node_set_basic( (NODE)if_node, NODE_E_IF, struct RNodeIf );
	}
	return (NODE)if_node;
}

NODE node_if_then_block_set( NODE if_node, NODE then_block )
{
	assert( if_node );
	assert( then_block );
	
	if( NODE_TYPE(if_node) != NODE_E_IF ) {
		fprintf( stderr, "type error - node_if_then_block_set\n" );
		abort();
	}
	NODE2IF(if_node)->then_block = then_block;
	
	return (NODE)if_node;
}

NODE node_if_else_block_set( NODE if_node, NODE else_block )
{
	assert( if_node );
	assert( else_block );
	
	if( NODE_TYPE(if_node) != NODE_E_IF ) {
		fprintf( stderr, "type error - node_if_else_block_set\n" );
		abort();
	}
	NODE2IF(if_node)->else_block = else_block;
	return (NODE)if_node;
}

NODE node_if_elsif_list_set( NODE if_node, NODE elsif_list )
{
	assert( if_node );
	assert( elsif_list );
	
	if( NODE_TYPE(if_node) != NODE_E_IF ) {
		fprintf( stderr, "type error(if_node) - node_if_elsif_list_set\n" );
		abort();
	}
	if( NODE_TYPE(elsif_list) != NODE_E_LIST ) {
		fprintf( stderr, "type error(elseif_list) - node_if_elsif_list_set\n" );
		abort();
	}
	NODE2IF(if_node)->elsif_list = elsif_list;
	return (NODE)if_node;
}

NODE node_assign_new( NODE left_nd, NODE operand_node )
{
	struct RNodeAssign* assign_node;

	assert( left_nd );
	assert( operand_node );

	assign_node = (struct RNodeAssign*)malloc( sizeof( struct RNodeAssign ) );
	{
		assign_node->left_nd = left_nd;
		assign_node->operand   = operand_node;

		node_set_basic( (NODE)assign_node, NODE_E_ASSIGN, struct RNodeAssign );
	}
	return (NODE)assign_node;
}


NODE node_symbol_new( char *symbol )
{
	struct RNodeSymbol *symbol_node;
	
	assert( symbol );
	
	symbol_node = (struct RNodeSymbol*)malloc( sizeof( struct RNodeSymbol ) );
	{
		symbol_node->symbol = rabbit_name2tag( symbol );

		node_set_basic( (NODE)symbol_node, NODE_E_SYMBOL, struct RNodeSymbol );
	}
	return (NODE)symbol_node;
}

NODE node_params_new( char* symbol )
{
	struct RNodeParams* params_node;
	
	assert( symbol );
	
	params_node = (struct RNodeParams*)malloc( sizeof( struct RNodeParams ) );
	{
		params_node->symbol = rabbit_name2tag( symbol );
		params_node->next = (NODE)NULL;
		
		node_set_basic( (NODE)params_node, NODE_E_PARAMS, struct RNodeParams );
	}
	return (NODE)params_node;
}

NODE node_params_add( NODE params_node, char* symbol )
{
	NODE new_params_node;
	NODE last;
	
	assert( params_node );
	assert( symbol );
	
	new_params_node = node_params_new( symbol );
	
	last = params_node;

	while(1) {
		if( NODE2PARAMS(last)->next == (NODE)NULL ) {
			break;
		}
		last = NODE2PARAMS(last)->next;
	}
	
	NODE2PARAMS(last)->next = new_params_node;
	
	return params_node;
}

NODE node_function_new( NODE params_node, NODE block_node )
{
	struct RNodeFunction* function_node;
	
	function_node = (struct RNodeFunction*)malloc( sizeof(struct RNodeFunction) );
	{
		function_node->params = params_node;
		function_node->block  = block_node;

		node_set_basic( (NODE)function_node, NODE_E_FUNCTION, struct RNodeFunction );
	}
	
	return (NODE)function_node;
}

NODE node_while_new( NODE condition, NODE block_node )
{
	struct RNodeWhile* while_node;
	
	while_node = (struct RNodeWhile*)malloc( sizeof( struct RNodeWhile) );
	{
		while_node->condition = condition;
		while_node->block = block_node;

		node_set_basic( (NODE)while_node, NODE_E_WHILE, struct RNodeWhile );
	}
	
	return (NODE)while_node;
}


