%{
#include "rabbit.h"
#include "node.h"
#include "parse.h"
#include "lexer.h"

#define YYDEBUG 1

VALUE eval_rabbit( NODE rabbit_tree );

%}
// ----------------------------------------------------------------------------
// こ こ か ら 構 文 規 則
// ----------------------------------------------------------------------------
%pure_parser
%verbose

%union {
	char  *identifier;
	NODE  node;
}
// -------------------------------------------------------------------
// -------------------------------------------------------------------
%token
	DOLLAR			"$"
	LF				"\n"
	SEMICOLON		";"
	COMMA			","
	DOT				"."
	BANG			"!"
	HATENA			"?"
	COLON			":"
	VERTICAL_BAR	"|"
	LC				"{"
	RC				"}"
	LB				"["
	RB				"]"
	tLP				"("
	RP				")"
	ASSIGN			"="
	ADD_ASSIGN		"+="
	SUB_ASSIGN		"-="
	MUL_ASSIGN		"*="
	DIV_ASSIGN		"/="
	MOD_ASSIGN		"%="
	EQ				"=="
	NE				"!="
	GT				">"
	LT				"<"
	GE				">="
	LE				"<="
	GTGT			">>"
	LTLT			"<<"
	IF				"if"
	THEN			"then"
	ELSE_IF			"elsif"
	ELSE			"else"
	END				"end"
	WHILE			"while"
	DO				"do"
	BREAK			"break"
	RETURN			"return"
	CLASS			"class"
	DEFUN			"def"
	YIELD			"yield"
	ADD				"+"
	SUB				"-"
	MUL				"*"
	DIV				"/"
	MOD				"%"
	LB_RB			"[]"
	LB_RB_ASSIGN	"[]="

%token <identifier> IDENTIFIER CALL_IDENTIFIER
%type  <identifier> Method_Identifier Operator
%token <node>       VALUE_LITERAL

%type <node>        AddSub_Expression
%type <node>        MulDiv_Expression
%type <node>        Primary_Expression
%type <node>        Expression
%type <node>        Statements
%type <node>        Statement
%type <node>        If_Expression
%type <node>        Cmp_Expression
%type <node>        Eq_Expression
%type <node>        Assign_Expression
%type <node>        Postfix_Expression
%type <node>        Shift_Expression
%type <node>        Arguments
%type <node>        Parameters
%type <node>        While_Expression
%type <node>        Ternary_Operation_Expression
%type <node>        Array_Literal
%type <node>        Class_Definition_Statement
%type <node>        Method_Definition_Statement
%type <node>        Array_Slotaccess_Expression
%type <node>        Object_Member
%type <node>        Break_Expression
%type <node>        Return_Expresssion
%type <node>        Unary_Expression
%type <node>        Elseif_List
%type <node>        Elseif
%type <node>        Method_Call_Expression
%type <node>        Iterator_Call_Expression
%type <node>        Block_Call_Statement


// -------------------------------------------------------------------
// -------------------------------------------------------------------
%%
Rabbit
	:
	{
		fprintf( stderr, "empty script\n" );
	}
	| Statements
	{
		eval_rabbit( $1 );
	}
;

Statements
	: Statement
	{
		if( $1 != (NODE)NULL ) {
			$$ = node_list_new( $1 );
		}
	}
	| Statements Statement
	{
		if( $1 == (NODE)NULL ) {
			$1 = node_list_new( $1 );
		}
		if( $2 != (NODE)NULL ) {
			$$ = node_list_add( $1, $2 );
		}
	}
;

Statement
	: error { YYABORT; }
	| "\n"  { $$ = (NODE)NULL; }
	| Expression "\n"
	| Expression ";"
	| Class_Definition_Statement
	| Method_Definition_Statement
	| Block_Call_Statement
	| Statement ";"
;

Expression
	: Assign_Expression
	| Break_Expression
	| Return_Expresssion
	| If_Expression
	| While_Expression
;

Assign_Expression
	: Ternary_Operation_Expression
	| Ternary_Operation_Expression "=" Assign_Expression
	{
		switch( NODE_TYPE($1) )
		{
		case NODE_E_SYMBOL:
			$$ = node_assign_new( $1, $3 );
			break;
		case NODE_E_METHOD_CALL:
		{
			// メソッド名に'='を追加する
			int len = strlen( rabbit_tag2name( NODE2MCALL($1)->method ) );
			char* metz = malloc( len + 2 );
			{
				strcpy( metz, rabbit_tag2name( NODE2MCALL($1)->method ) );
				metz[len] = '=';
				NODE2MCALL($1)->method = rabbit_name2tag( metz );
			}

			// 引数に右辺値を加える
			if( NODE2MCALL($1)->arguments != (NODE)NULL ) {
				NODE2MCALL($1)->arguments = node_list_add( NODE2MCALL($1)->arguments, $3 );
			} else {
				NODE2MCALL($1)->arguments = node_list_new( $3 );
			}

			$$ = $1;
			break;
		}
		default:
			fprintf( stderr, "[bug]parser error - 左辺値がよくわかりません(%d)\n", NODE_TYPE($1) );
			abort();
			break;
		}
	}
	| Ternary_Operation_Expression "+=" Assign_Expression
	{
		NODE mcall_nd = node_call_new( $1, "+", node_list_new($3), (NODE)NULL );
		$$ = node_assign_new( $1, mcall_nd );
	}
	| Ternary_Operation_Expression "-=" Assign_Expression
	{
		NODE mcall_nd = node_call_new( $1, "-", node_list_new($3), (NODE)NULL );
		$$ = node_assign_new( $1, mcall_nd );
	}
	| Ternary_Operation_Expression "*=" Assign_Expression
	{
		NODE mcall_nd = node_call_new( $1, "*", node_list_new($3), (NODE)NULL );
		$$ = node_assign_new( $1, mcall_nd );
	}
	| Ternary_Operation_Expression "/=" Assign_Expression
	{
		NODE mcall_nd = node_call_new( $1, "/", node_list_new($3), (NODE)NULL );
		$$ = node_assign_new( $1, mcall_nd );
	}
	| Ternary_Operation_Expression "%=" Assign_Expression
	{
		NODE mcall_nd = node_call_new( $1, "%", node_list_new($3), (NODE)NULL );
		$$ = node_assign_new( $1, mcall_nd );
	}
;

Ternary_Operation_Expression
	: Eq_Expression
	| Eq_Expression "?" Expression ":" Ternary_Operation_Expression
	{
		// 三項演算子
		$$ = node_ternary_operation_new( $1, $3, $5 );
	}
;

Eq_Expression
	: Cmp_Expression
	| Eq_Expression "==" Cmp_Expression
	{
		$$ = node_call_new( $1, "==", node_list_new($3), (NODE)NULL );
	}
	| Eq_Expression "!=" Cmp_Expression
	{
		$$ = node_call_new( $1, "!=", node_list_new($3), (NODE)NULL );
	}
;

Cmp_Expression
	: Shift_Expression
	| Cmp_Expression ">" Shift_Expression
	{
		$$ = node_call_new( $1, ">", node_list_new($3), (NODE)NULL );
	}
	| Cmp_Expression "<" Shift_Expression
	{
		$$ = node_call_new( $1, "<", node_list_new($3), (NODE)NULL );
	}
	| Cmp_Expression ">=" Shift_Expression
	{
		$$ = node_call_new( $1, ">=", node_list_new($3), (NODE)NULL );
	}
	| Cmp_Expression "<=" Shift_Expression
	{
		$$ = node_call_new( $1, "<=", node_list_new($3), (NODE)NULL );
	}
;

Shift_Expression
	: AddSub_Expression
	| Shift_Expression "<<" AddSub_Expression
	{
		$$ = node_call_new( $1, "<<", node_list_new($3), (NODE)NULL );
	}
	| Shift_Expression ">>" AddSub_Expression
	{
		$$ = node_call_new( $1, ">>", node_list_new($3), (NODE)NULL );
	}
;

AddSub_Expression
	: MulDiv_Expression
	| AddSub_Expression "+" MulDiv_Expression
	{
		$$ = node_call_new( $1, "+", node_list_new($3), (NODE)NULL );
	}
	| AddSub_Expression "-" MulDiv_Expression
	{
		$$ = node_call_new( $1, "-", node_list_new($3), (NODE)NULL );
	}
;

MulDiv_Expression
	: Unary_Expression
	| MulDiv_Expression "*" Unary_Expression
	{
		$$ = node_call_new( $1, "*", node_list_new($3), (NODE)NULL );
	}
	| MulDiv_Expression "/" Unary_Expression
	{
		$$ = node_call_new( $1, "/", node_list_new($3), (NODE)NULL );
	}
	| MulDiv_Expression "%" Unary_Expression
	{
		$$ = node_call_new( $1, "%", node_list_new($3), (NODE)NULL );
	}
;

Unary_Expression
	: Postfix_Expression
	| "-" Unary_Expression
	{
		$$ = node_call_new( $2, "-@", (NODE)NULL, (NODE)NULL );
	}
	| "!" Unary_Expression
	{
		$$ = node_logical_not_new( $2 );
	}
;

Postfix_Expression
	: Primary_Expression
;

Primary_Expression
	: "(" Expression ")"
	{
		$$ = $2;
	}
	| IDENTIFIER
	{
		$$ = node_symbol_new( $1 );
	}
	| "yield"
	{
		$$ = node_yield_new( (NODE)NULL );
	}
	| "yield" "(" ")"
	{
		$$ = node_yield_new( (NODE)NULL );
	}
	| "yield" "(" Arguments ")"
	{
		$$ = node_yield_new( $3 );
	}
	| VALUE_LITERAL
	| Array_Literal
	| Array_Slotaccess_Expression
	| Iterator_Call_Expression
	| Method_Call_Expression
;

Array_Slotaccess_Expression
	: Primary_Expression "[" Expression "]"
	{
		NODE list_nd = node_list_new( $3 );
		$$ = node_call_new( $1, "[]", list_nd, (NODE)NULL );
	}
;


While_Expression
	: "while" Expression "do" Statements "end"
	{
		$$ = node_while_new( $2, $4 );
	}
;

Block_Call_Statement
	: Method_Call_Expression "do" Statements "end"
	{
		$$ = node_method_call_block_set( $1, (NODE)NULL, $3 );
	}
	| Method_Call_Expression "do" "|" Parameters "|" Statements "end"
	{
		$$ = node_method_call_block_set( $1, $4, $6 );
	}
;

Iterator_Call_Expression
	: Method_Call_Expression "{" Statements "}"
	{
		$$ = node_method_call_block_set( $1, (NODE)NULL, $3 );
	}
	| Method_Call_Expression "{" "|" Parameters "|" Statements "}"
	{
		$$ = node_method_call_block_set( $1, $4, $6 );
	}
;

Method_Call_Expression
	: Object_Member "(" Arguments ")"
	{
		$$ = node_call_new( NODE2RECV($1)->node, NODE2RECV($1)->method_name, $3, (NODE)NULL );
	}
	| Object_Member
	{
		$$ = node_call_new( NODE2RECV($1)->node, NODE2RECV($1)->method_name, (NODE)NULL, (NODE)NULL );
	}
	| CALL_IDENTIFIER
	{
		$$ = node_call_new( (NODE)NULL, $1, (NODE)NULL, (NODE)NULL );
	}
	| Object_Member "(" ")"
	{
		$$ = node_call_new( NODE2RECV($1)->node, NODE2RECV($1)->method_name, (NODE)NULL, (NODE)NULL );
	}
	| Method_Identifier "(" Arguments ")"
	{
		$$ = node_call_new( (NODE)NULL, $1, $3, (NODE)NULL );
	}
	| Method_Identifier "(" ")"
	{
		$$ = node_call_new( (NODE)NULL, $1, (NODE)NULL, (NODE)NULL );
	}
;

Object_Member
	: Primary_Expression "." Method_Identifier
	{
		$$ = node_recv_new( $1, $3 );
	}
	| Primary_Expression "." Operator
	{
		$$ = node_recv_new( $1, $3 );
	}
;

Class_Definition_Statement
	: "class" Method_Identifier Statements "end"
	{
		$$ = node_class_defin_new( $2, $3, (NODE)NULL );
	}
	| "class" Method_Identifier "<" IDENTIFIER Statements "end"
	{
		$$ = node_class_defin_new( $2, $5, $4 );
	}
;

Method_Definition_Statement
	: "def" Method_Identifier "(" Parameters ")" Statements  "end"
	{
		$$ = node_method_defin_new( $2, $4, $6 );
	}
	| "def" Method_Identifier "(" ")" Statements  "end"
	{
		$$ = node_method_defin_new( $2, (NODE)NULL, $5 );
	}
	| "def" Operator "(" ")" Statements  "end"
	{
		$$ = node_method_defin_new( $2, (NODE)NULL, $5 );
	}
	| "def" Operator "(" Parameters ")" Statements  "end"
	{
		$$ = node_method_defin_new( $2, $4, $6 );
	}
;

Method_Identifier
	: IDENTIFIER
	| CALL_IDENTIFIER
;

Operator
	: "+"    { $$ = rabbit_tag2name( rabbit_name2tag("+")   ); }
	| "-"    { $$ = rabbit_tag2name( rabbit_name2tag("-")   ); }
	| "*"    { $$ = rabbit_tag2name( rabbit_name2tag("*")   ); }
	| "/"    { $$ = rabbit_tag2name( rabbit_name2tag("/")   ); }
	| "%"    { $$ = rabbit_tag2name( rabbit_name2tag("%")   ); }
	| "[]"   { $$ = rabbit_tag2name( rabbit_name2tag("[]")  ); }
	| "[]="  { $$ = rabbit_tag2name( rabbit_name2tag("[]=") ); }
	| ">>"   { $$ = rabbit_tag2name( rabbit_name2tag(">>")  ); }
	| "<<"   { $$ = rabbit_tag2name( rabbit_name2tag("<<")  ); }
	| "<"    { $$ = rabbit_tag2name( rabbit_name2tag("<")  );  }
	| ">"    { $$ = rabbit_tag2name( rabbit_name2tag(">")  );  }
	| "<="   { $$ = rabbit_tag2name( rabbit_name2tag("<=")  ); }
	| ">="   { $$ = rabbit_tag2name( rabbit_name2tag(">=")  ); }
	| "=="   { $$ = rabbit_tag2name( rabbit_name2tag("==")  ); }
	| "!="   { $$ = rabbit_tag2name( rabbit_name2tag("!=")  ); }
;

Return_Expresssion
	: "return"
	{
		$$ = node_return_new( (NODE)NULL );
	}
	| "return" Expression
	{
		$$ = node_return_new( $2 );
	}
;

Break_Expression
	: "break"
	{
		$$ = node_break_new( (NODE)NULL );
	}
	| "break" "if" Expression
	{
		$$ = node_break_new( $3 );
	}
;

Array_Literal
	: "[" Arguments "]"
	{
		$$ = node_array_literal_new( $2 );
	}
	| "[" Arguments "," "]"
	{
		$$ = node_array_literal_new( $2 );
	}
	| "[" "]"
	{
		$$ = node_array_literal_new( (NODE)NULL );
	}
	| "[" "," "]"
	{
		$$ = node_array_literal_new( (NODE)NULL );
	}
;

Arguments
	: Expression
	{
		$$ = node_list_new( $1 );
	}
	| Arguments "," Expression
	{
		$$ = node_list_add( $1, $3 );
	}
;

Parameters
	: IDENTIFIER
	{
		$$ = node_params_new( $1 );
	}
	| Parameters COMMA IDENTIFIER
	{
		$$ = node_params_add( $1, $3 );
	}
;

If_Expression
	: "if" Expression "then" Statements "end"
	{
		$$ = node_if_new( $2 );
		if( $4 != (NODE)NULL ) {
			$$ = node_if_then_block_set( $$, $4 );
		}
	}
	| "if" Expression "then" Statements "else" Statements "end"
	{
		$$ = node_if_new( $2 );
		if( $4 != (NODE)NULL ) {
			$$ = node_if_then_block_set( $$, $4 );
		}
		if( $6 != (NODE)NULL ) {
			$$ = node_if_else_block_set( $$, $6 );
		}
	}
	| "if" Expression "then" Statements Elseif_List  "end"
	{
		$$ = node_if_new( $2 );
		if( $4 != (NODE)NULL ) {
			$$ = node_if_then_block_set( $$, $4 );
		}
		if( $5 != (NODE)NULL ) {
			$$ = node_if_elsif_list_set( $$, $5 );
		}
	}
	| "if" Expression "then" Statements Elseif_List "else" Statements "end"
	{
		$$ = node_if_new( $2 );

		if( $4 != (NODE)NULL ) {
			$$ = node_if_then_block_set( $$, $4 );
		}
		if( $5 != (NODE)NULL ) {
			$$ = node_if_elsif_list_set( $$, $5 );
		}
		if( $7 != (NODE)NULL ) {
			$$ = node_if_else_block_set( $$, $7 );
		}
	}
;

Elseif_List
	: Elseif
	{
		$$ = node_list_new( $1 );
	}
	| Elseif_List Elseif
	{
		$$ = node_list_add( $1, $2 );
	}
;

Elseif
	: "elsif" Expression "then" Statements
	{
		$$ = node_if_new( $2 );
		if( $4 != (NODE)NULL ) {
			$$ = node_if_then_block_set( $$, $4 );
		}
	}
;

%%

