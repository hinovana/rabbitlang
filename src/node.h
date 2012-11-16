#ifndef RABBIT_NODE_H_
#define RABBIT_NODE_H_

// ------------------------------------------------------------------
// NODE
// ------------------------------------------------------------------
typedef unsigned long NODE;


enum rabbit_node_type {
	NODE_E_UNKNOWN = 1,
	NODE_E_NULL,
	NODE_E_IDENTIFIER,
	NODE_E_VALUE,
	NODE_E_ASSIGN,
	NODE_E_LIST,
	NODE_E_IF,
	NODE_E_FUNCTION,
	NODE_E_BREAK,
	NODE_E_RETURN,
	NODE_E_SYMBOL,
	NODE_E_PARAMS,
	NODE_E_WHILE,
	NODE_E_TERNARY, // condition ? expr : expr
	NODE_E_METHOD_CALL,
	NODE_E_ARRAY_LITERAL,
	NODE_E_CLASS_DEFIN,
	NODE_E_METHOD_DEFIN,
	NODE_E_RECV,
	NODE_E_YIELD,
	NODE_E_LOGNOT,
};

#define NODE_TYPE_NAME(e) 								\
	  e == NODE_E_UNKNOWN       ? "UNKNOWN"				\
	: e == NODE_E_NULL          ? "NULL"				\
	: e == NODE_E_IDENTIFIER    ? "IDENTIFIER"			\
	: e == NODE_E_VALUE         ? "VALUE"				\
	: e == NODE_E_ASSIGN        ? "ASSIGN"				\
	: e == NODE_E_LIST          ? "LIST"				\
	: e == NODE_E_IF            ? "IF"					\
	: e == NODE_E_FUNCTION      ? "FUNCTION"			\
	: e == NODE_E_BREAK         ? "BREAK"				\
	: e == NODE_E_RETURN        ? "RETURN"				\
	: e == NODE_E_SYMBOL        ? "SYMBOL"				\
	: e == NODE_E_PARAMS        ? "PARAMS"				\
	: e == NODE_E_WHILE         ? "WHILE"				\
	: e == NODE_E_TERNARY       ? "TERNARY"				\
	: e == NODE_E_METHOD_CALL   ? "METHOD_CALL"			\
	: e == NODE_E_ARRAY_LITERAL ? "ARRAY_LITERAL"		\
	: e == NODE_E_CLASS_DEFIN   ? "CLASS_DEFIN"			\
	: e == NODE_E_METHOD_DEFIN  ? "METHOD_DEFIN"		\
	: e == NODE_E_RECV          ? "RECV"				\
	: e == NODE_E_YIELD         ? "YIELD"				\
	: e == NODE_E_LOGNOT        ? "LOGNOT"				\
	: "(other)"


enum rabbit_calc_type {
	CALC_E_ADD = 1,
	CALC_E_SUB,
	CALC_E_MUL,
	CALC_E_DIV,
	CALC_E_MOD,
};

enum rabbit_cmp_type {
	CMP_E_EQ = 1,
	CMP_E_NE,
	CMP_E_GT,
	CMP_E_LT,
	CMP_E_GE,
	CMP_E_LE,
};

enum rabbit_assign_type {
	ASSIGN_E_ASSIGN = 1,
	ASSIGN_E_ADD,
	ASSIGN_E_SUB,
	ASSIGN_E_MUL,
	ASSIGN_E_DIV,
	ASSIGN_E_MOD,
};


struct RNode {
	enum rabbit_node_type type;
	unsigned long line;
};

struct RNodeList {
	struct RNode basic;
	NODE node;
	NODE next;
	size_t len;
};

struct RNodeLognot {
	struct RNode basic;
	NODE expr;
};

struct RNodeSymbol {
	struct RNode basic;
	TAG symbol;
};

// evalにバグがあって。symbolとnextの順序を入れ替えるだけでバグるのでどうにかすること。
// ↑治した
struct RNodeParams {
	struct RNode basic;
	TAG  symbol;
	NODE next;
};

struct RNodeClass {
	struct RNode basic;
	TAG  symbol;
	NODE block;
	TAG  super;
	const char* identifier;
};

struct RNodeMethod {
	struct RNode basic;
	TAG  symbol;
	const char* identifier;
	NODE params;
	NODE block;
};


struct RNodeValue {
	struct RNode basic;
	VALUE value;
};

struct RNodeLiteral {
	struct RNode basic;
	NODE node;
};

struct RNodeCalc {
	struct RNode basic;
	enum rabbit_calc_type type;
	NODE left;
	NODE right;
};

struct RNodeAssign {
	struct RNode basic;
	NODE left_nd;
	NODE operand;
};

struct RNodeOrder {
	struct RNode basic;
	TAG  symbol;
	NODE params;
};

struct RNodeCmp {
	struct RNode basic;
	enum rabbit_cmp_type type;
	NODE left;
	NODE right;
};

struct RNodeIf {
	struct RNode basic;
	NODE condition;
	NODE then_block;
	NODE elsif_list;
	NODE else_block;
};

struct RNodeFunction {
	struct RNode basic;
	NODE params;
	NODE block;
};

struct RNodeMassign {
	struct RNode basic;
	NODE  object;
	TAG  symbol;
	NODE operand;
};

struct RNodeWhile {
	struct RNode basic;
	NODE condition;
	NODE block;
};

struct RNodeBreak {
	struct RNode basic;
	NODE condition;
};

struct RNodeReturn {
	struct RNode basic;
	NODE expr;
};

NODE node_return_new( NODE expr );
NODE node_break_new( NODE condition );


struct RNodeRecv {
	struct RNode basic;
	NODE node;
	const char* method_name;
};

struct RNodeMcall {
	struct RNode basic;
	NODE recv;
	TAG  method;
	NODE arguments;
	NODE func_block;
};

struct RNodeYield {
	struct RNode basic;
	NODE arguments;
};

NODE node_method_call_block_set( NODE mcall_nd, NODE params_nd, NODE list_nd );
NODE node_yield_new( NODE list_nd );

#define NODE_TYPE(x)  (((struct RNode*)(x))->type)

#define NODE2VALUE(x) (((struct RNodeValue*)(x))->value)

#define NODE_CAST(st)   (struct st *)
#define NODE2BASIC(nd)  (NODE_CAST(RNode)nd)
#define NODE2LIST(nd)   (NODE_CAST(RNodeList)nd)
#define NODE2CALC(nd)   (NODE_CAST(RNodeCalc)nd)
#define NODE2CMP(nd)    (NODE_CAST(RNodeCmp)nd)
#define NODE2IF(nd)     (NODE_CAST(RNodeIf)nd)
#define NODE2SYMBOL(nd) (NODE_CAST(RNodeSymbol)nd)
#define NODE2PARAMS(nd) (NODE_CAST(RNodeParams)nd)
#define NODE2ASSIGN(nd) (NODE_CAST(RNodeAssign)nd)
#define NODE2CALL(nd)   (NODE_CAST(RNodeCall)nd)
#define NODE2FUNC(nd)   (NODE_CAST(RNodeFunction)nd)
#define NODE2WHILE(nd)  (NODE_CAST(RNodeWhile)nd)
//#define NODE2OBJECT(nd) (NODE_CAST(RNodeObject)nd)
#define NODE2RECV(nd)   (NODE_CAST(RNodeRecv)nd)
#define NODE2CLASS(nd) ((struct RNodeClass*)nd)
#define NODE2METHOD(nd) ((struct RNodeMethod*)nd)
#define NODE2LITERAL(nd) ((struct RNodeLiteral*)nd)
#define NODE2MCALL(nd) ((struct RNodeMcall*)nd)
#define NODE2BREAK(nd) ((struct RNodeBreak*)nd)
#define NODE2RETURN(nd) ((struct RNodeReturn*)nd)
#define NODE2BLOCK(nd)  (NODE_CAST(RNodeBlock)nd)
#define NODE2YIELD(nd)  (NODE_CAST(RNodeYield)nd)
#define NODE2LOGNOT(nd)  (NODE_CAST(RNodeLognot)nd)

NODE node_new( enum rabbit_node_type );
NODE node_list_new( NODE first_node );
NODE node_list_add( NODE list_node, NODE add_node );
NODE node_params_new( char* symbol );
NODE node_params_add( NODE params_node, char* symbol );
NODE node_function_new( NODE params_node, NODE block_node );
NODE node_while_new( NODE condition, NODE block_node );
NODE node_intvalue_new( int fixnum );
NODE node_truevalue_new();
NODE node_falsevalue_new();
NODE node_stringvalue_new( char* strval );
NODE node_array_literal_new( NODE arguments );
NODE node_symbol_new( char *symbol );

NODE node_if_new( NODE condition );
NODE node_if_then_block_set( NODE if_node, NODE then_block );
NODE node_if_else_block_set( NODE if_node, NODE else_block );
NODE node_if_elsif_list_set( NODE if_node, NODE elsif_list );

NODE node_increment_new( NODE primary_node );

//NODE node_assign_new( char *identifier, NODE operand_node, int is_setglobal );
//NODE node_assign_new( enum rabbit_assign_type, NODE left_nd, NODE operand_node );
NODE node_assign_new( NODE left_nd, NODE operand_node );

NODE node_ternary_operation_new( NODE condition, NODE then_node, NODE else_node );

// クラス定義
NODE node_class_defin_new( char* symbol, NODE block, char* super );
// メソッド定義
NODE node_method_defin_new( char* symbol, NODE params, NODE block );

// メソッド呼び出し
//NODE node_call_new( NODE recv_nd, char* method, NODE arguments_nd );
NODE node_call_new( NODE recv_nd, const char* method_name, NODE arguments_nd, NODE func_block_nd );

// レシーバノード
NODE node_recv_new( NODE object, const char *method_name );

NODE node_logical_not_new( NODE expr );

#endif // RABBIT_NODE_H_
