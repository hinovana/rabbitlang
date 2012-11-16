#include <setjmp.h>
#include <assert.h>
#include "st.h"
#include "rabbit.h"

void gc_work();

// mallocがこのサイズを超えたらGCする
#define GC_MALLOC_LIMIT 8000000

// オブジェクトが、これ以上生成されたらGCする
#define GC_NEWOBJ_LIMIT 10000

static unsigned long rabbit_malloc_total = 0;
static unsigned long rabbit_object_total = 0;
static unsigned long malloc_memory_increase = 0;
static unsigned long malloc_object_increase = 0;
static unsigned long GC_work_count = 0;
static unsigned int  GC_STOP = 0;


VALUE *rabbit_gc_stack_start;

/**
 * マクロ書くのめんどくさかったんでrubyからパクりました。
 */
#ifdef C_ALLOCA
#   define SET_STACK_END VALUE stack_end;
#   define STACK_END (&stack_end)
#else
#   if defined(__GNUC__) && defined(USE_BUILTIN_FRAME_ADDRESS)
#      define SET_STACK_END  VALUE *stack_end = __builtin_frame_address(0)
#   else
#      define SET_STACK_END  VALUE *stack_end = alloca(1)
#   endif
#   define STACK_END (stack_end)
#endif

#if defined(sparc) || defined(__sparc__)
#   if defined(linux) || defined(__linux__)
#      define FLUSH_REGISTER_WINDOWS  asm("ta  0x83")
#   else /* Solaris, not sparc linux */
#      define FLUSH_REGISTER_WINDOWS  asm("ta  0x03")
#   endif
#else /* Not a sparc */
#   define FLUSH_REGISTER_WINDOWS
#endif


void *rabbit_malloc( size_t size )
{
	void* ptr;
	
	if( malloc_memory_increase >= GC_MALLOC_LIMIT ) {
		gc_work();
	}
	if( (ptr = malloc( size )) == NULL ) {
		fprintf( stderr, "malloc faild" );
		abort();
	}
	malloc_memory_increase += size;
	rabbit_malloc_total += size;

	return ptr;
}

void *rabbit_calloc( size_t n, size_t size )
{
	void* ptr;
	
	if( malloc_memory_increase >= GC_MALLOC_LIMIT ) {
		gc_work();
	}
	if( (ptr = calloc( n, size )) == NULL ) {
		fprintf( stderr, "calloc faild" );
		abort();
	}
	malloc_memory_increase += size;
	rabbit_malloc_total += size;

	return ptr;
}

void *rabbit_realloc( void* ptr, size_t size )
{
	void* new_ptr;
	
	if( (new_ptr = realloc( ptr, size )) == NULL ) {
		fprintf( stderr, "realloc faild\n" );
		abort();
	}

	malloc_memory_increase += size;
	rabbit_malloc_total += size;
	
	return new_ptr;
}

// ------------------------------------------------------------------
// GC
// ------------------------------------------------------------------

struct gc_list {
	VALUE  *value;
	struct gc_list *next;
};

struct object_list {
	VALUE value;
	struct object_list *next;
};

struct gc_list     *GC_Global_List = NULL;
struct object_list *Literal_List   = NULL;

void gc_mark( VALUE value );
void gc_sweep();
void rabbit_object_free( VALUE value );

void rabbit_gc_global( VALUE *value )
{
	struct gc_list *first;
	
	first = rabbit_malloc( sizeof(struct gc_list) );
	first->next  = GC_Global_List;
	first->value = value;

	GC_Global_List = first;
}

void rabbit_gc_literal( VALUE value )
{
	struct object_list *first;
	
	first = rabbit_malloc( sizeof(struct object_list) );
	first->next  = Literal_List;
	first->value = value;

	Literal_List = first;
}

typedef struct RVALUE {
	union {
		struct {
			FLAG   flags;
			struct RVALUE *next;
		} free;
		struct RBasic   basic;
		struct RObject  object;
		struct RClass   klass;
		struct RString  string;
		struct RArray   array;
		struct RMethod  method;
	} as;
} RVALUE;


#define RANY(o) ((RVALUE*)(o))

// ヒープのスロット数
#define RABBIT_HEAP_SLOT_LEN  10000

// ヒープが足りなくなったら何個追加するか
#define RABBIT_HEAP_INCREMENT 10

#define RABBIT_HEAP_FREE_MIN  4096

static RVALUE **rabbit_heaps;
static unsigned int rabbit_heaps_len  = 0;
static unsigned int rabbit_heaps_used = 0;

static RVALUE *heap_low_ptr;
static RVALUE *heap_high_ptr;

static RVALUE *free_list = 0;

unsigned long free_len();

void rabbit_heap_stretch()
{
	RVALUE *ptr;
	RVALUE *ptr_end;
	
	// ヒープが足りなくなったら伸ばす
	if( rabbit_heaps_len == rabbit_heaps_used ) {
		rabbit_heaps_len += RABBIT_HEAP_INCREMENT;
		rabbit_heaps = realloc( rabbit_heaps,        RABBIT_HEAP_SLOT_LEN + sizeof(RVALUE*) );
	}
//	fprintf( stderr, "heap stretch *************************************************************************\n" );
	
	// ヒープを1個増やす
	ptr     = rabbit_heaps[ rabbit_heaps_used ] = malloc( sizeof(RVALUE) * RABBIT_HEAP_SLOT_LEN );
	ptr_end = ptr + RABBIT_HEAP_SLOT_LEN;
	
	rabbit_malloc_total += sizeof(RVALUE) * RABBIT_HEAP_SLOT_LEN;
	
	if( heap_low_ptr == 0 || heap_low_ptr > ptr ) {
		heap_low_ptr = ptr;
	}
	if( heap_high_ptr < ptr_end ) {
		heap_high_ptr = ptr_end;
	}
	
	rabbit_heaps_used++;
	
	while( ptr < ptr_end ) {
		ptr->as.free.flags = 0;
		ptr->as.free.next  = free_list;
		free_list = ptr;
		ptr++;
	}
}

unsigned long free_len()
{
	RVALUE *ptr = free_list;
	unsigned long len = 0;
	
	if(!free_list) {
		return 0;
	}
	while( ptr ) {
		++len;
		ptr = ptr->as.free.next;
	}
	return len;
}

VALUE rabbit_gc_new_object()
{
	VALUE obj;
	
	if( !free_list ) {
		gc_work();
	}
	if( free_list->as.free.flags != 0 ) {
		rabbit_bug( "free objectなのになぜかフラグが立ってました(%ld)", free_list->as.free.flags );
	}
	
	obj = (VALUE)free_list;
	free_list = free_list->as.free.next;
	memset( (void*)obj, 0, sizeof(RVALUE) );
	
	++rabbit_object_total;

	return obj;
}

int is_pointer_to_heap_value( void *nazo_ptr )
{
	RVALUE *ptr = RANY(nazo_ptr);
	unsigned long n = 0;
	RVALUE *heap_start, *heap_end;

	if( ptr < heap_low_ptr || ptr > heap_high_ptr ) {
		return R_FALSE;
	}
	
	for( n=0; n<rabbit_heaps_used; ++n ) {
		heap_start = rabbit_heaps[n];
		heap_end   = heap_start + RABBIT_HEAP_SLOT_LEN;
		
		if( heap_start <= ptr && ptr < heap_end ) {
			if( ((((char*)ptr)-((char*)heap_start))%sizeof(RVALUE)) == 0) {
				return R_TRUE;
			}
		}
	}
	return R_FALSE;
}

void gc_stack_mark( VALUE *start, VALUE *end )
{
	VALUE *ptr;
	long n;
	
	if( start > end ) {
		rabbit_bug2("startのほうが大きい");
	}
	n   = end - start;
	ptr = start;
	
	while(n--) {
		if( is_pointer_to_heap_value((void*)*ptr) ) {
			gc_mark(*ptr);
		}
		ptr++;
	}
}


static int var_tbl_mark_iter__( TAG id, VALUE value )
{
	gc_mark(value);
	return ST_CONTINUE;
}

void var_tbl_mark( st_table *var_tbl )
{
	if( var_tbl != (st_table*)NULL ) {
		st_foreach( var_tbl, var_tbl_mark_iter__, 0 );
	}
}

void scope_tbl_mark( VALUE scope )
{
	if( R_SCOPE(scope)->var_tbl != (st_table*)NULL ) {
		st_foreach( R_SCOPE(scope)->var_tbl, var_tbl_mark_iter__, 0 );
	}
}

void gc_work()
{
	extern struct st_table *class_tbl;

	jmp_buf save_regs_gc_mark;

	SET_STACK_END;
	
	malloc_memory_increase = 0;
	malloc_object_increase = 0;

	if( GC_STOP ) {
		rabbit_heap_stretch();
		return;
	}
	
	++GC_work_count;

	// クラスをマークする
	var_tbl_mark( class_tbl );

	// 使用中のスコープから参照できるオブジェクトをマークする
	active_scope_foreach( scope_tbl_mark );

	{ // リテラルをマークする
		struct object_list *next;

		next = Literal_List;
		
		while( next != NULL ) {
			if( !IS_PTR_VALUE(next->value) ) {
				rabbit_bug( "(GC)ポインタ型では無いオブジェクトがリテラルリストにいました(%d)", VALUE_TYPE(next->value) );
			}
			gc_mark(next->value);
			next = next->next;
		}
	}

	{ // グローバル変数をマークする
		struct gc_list *next;

		for( next = GC_Global_List; next != NULL; next = next->next ) {
			gc_mark(*next->value);
		}
	}
	
	// Cのスタックをマークする
	if( (VALUE*)STACK_END < rabbit_gc_stack_start ) {
		gc_stack_mark( (VALUE*)STACK_END, rabbit_gc_stack_start );
	} else {
		gc_stack_mark( rabbit_gc_stack_start, (VALUE*)STACK_END+1 );
	}
	

	{ // レジスタ内をマークする
		FLUSH_REGISTER_WINDOWS;
		setjmp(save_regs_gc_mark);

		VALUE *ptr = (VALUE*)save_regs_gc_mark;
		long n = sizeof(save_regs_gc_mark) / sizeof(VALUE *);

		while (n--) {
			if( is_pointer_to_heap_value((void*)*ptr) ) {
				gc_mark(*ptr);
			}
			ptr++;
		}
	}

	gc_sweep();
}

void gc_mark( VALUE value )
{
	if( !IS_PTR_VALUE(value) ) {
		return;
	}
	if( FLAG_TEST( value, FLAG_GC_MARK ) ) {
		return;
	}
	if( RANY(value)->as.free.flags == 0 ) {
		return;
	}
	FLAG_SET( value, FLAG_GC_MARK );

	gc_mark( R_BASIC(value)->klass );

	switch(VALUE_TYPE(value))
	{
	case TYPE_STRING:
		break;

	case TYPE_ARRAY: {
		int i;
		for( i=0; i< R_ARRAY(value)->len; ++i ) {
			gc_mark( R_ARRAY(value)->ptr[i] );
		}
		break;
	}
	case TYPE_CLASS:
		gc_mark( (VALUE)(R_CLASS(value)->super) );
		var_tbl_mark( R_CLASS(value)->ival_tbl );
		var_tbl_mark( R_CLASS(value)->method_tbl );
		break;

	case TYPE_OBJECT:
		gc_mark( (VALUE)(R_BASIC(value)->klass) );
		var_tbl_mark( R_OBJECT(value)->ival_tbl );
		break;

	case TYPE_METHOD:
		break;

	default:
		rabbit_bug("(GC MARK)未対応型のオブジェクトが送られてきました(type %d)", VALUE_TYPE(value) );
	}

}

void obj_free( VALUE val );

void gc_sweep()
{
	unsigned long heap_n;
	RVALUE *ptr, *ptr_end;
	int n = 0;
	
	for( heap_n = 0; heap_n < rabbit_heaps_used; ++heap_n ) {
		ptr     = rabbit_heaps[ heap_n ];
		ptr_end = ptr + RABBIT_HEAP_SLOT_LEN;
		
		while( ptr < ptr_end ) {
			if( !(ptr->as.free.flags & FLAG_GC_MARK) ) {
				if( ptr->as.free.flags ) {
					obj_free( (VALUE)ptr );
					ptr->as.free.flags = 0;
					ptr->as.free.next  = free_list;
					free_list = ptr;
					++n;
				}
			} else {
				ptr->as.free.flags &= ~FLAG_GC_MARK;
			}
			ptr++;
		}
	}

	if( free_len() < RABBIT_HEAP_FREE_MIN ) {
		rabbit_heap_stretch();
	}
}

void obj_free( VALUE object )
{
	int type;
	
	assert( object );
	
	switch(RANY(object)->as.free.flags & TYPE_MASK)
	{
	case TYPE_NONE:
	case TYPE_NULL:
	case TYPE_TRUE:
	case TYPE_FALSE:
	case TYPE_FIXNUM:
		rabbit_bug("ポインタでは無い値をfreeしようとしました。(type .. %d)", RANY(object)->as.free.flags );
	}

#define PUTS_SWEEP_TYPE(s) fprintf( stderr, "sweep object .. %s\n", s )

	type = RANY(object)->as.free.flags & TYPE_MASK;
	
	switch(type)
	{
	case TYPE_OBJECT:
		st_free_table( RANY(object)->as.object.ival_tbl );
		break;

	case TYPE_ARRAY:
		free( RANY(object)->as.array.ptr );
		break;

	case TYPE_STRING:
		free( RANY(object)->as.string.ptr );
		break;

	case TYPE_HASH:
		PUTS_SWEEP_TYPE("TYPE_HASH");
	case TYPE_SCOPE:
		PUTS_SWEEP_TYPE("TYPE_SCOPE");
	case TYPE_CLASS:
		PUTS_SWEEP_TYPE("TYPE_CLASS");
		rabbit_bug( "cname = %d", RANY(object)->as.klass.symbol );
	case TYPE_METHOD:
		PUTS_SWEEP_TYPE("TYPE_METHOD");
		rabbit_bug( "type = %d", type );
	}


#undef  PUTS_SWEEP_TYPE
}

void rabbit_object_free( VALUE value )
{
	assert( value );

	if( !IS_PTR_VALUE(value) ) {
		rabbit_bug("(object_free)ポインタ型では無いオブジェクトが送られてきました(type %d)", VALUE_TYPE(value) );
	}

	switch(VALUE_TYPE(value)) {
	case TYPE_STRING:
		free( R_STRING(value)->ptr );
		break;

	case TYPE_ARRAY:
		free( R_ARRAY(value)->ptr );
		break;

	case TYPE_CLASS:
		break;

	case TYPE_OBJECT:
		st_free_table( R_OBJECT(value)->ival_tbl );
		R_OBJECT(value)->ival_tbl = 0;
		break;

	case TYPE_METHOD:
		break;

	default:
		rabbit_bug("(object_free)未対応型のオブジェクトが送られてきました(type %d)", VALUE_TYPE(value) );
	}

	free( (void*)(value) );
}

VALUE rabbit_gc_start( VALUE self )
{
	gc_work();

	return R_NULL;
}

VALUE rabbit_gc_alloc_total( VALUE self )
{
	return INT2FIX( rabbit_malloc_total );
}

VALUE rabbit_gc_object_total( VALUE self )
{
	return INT2FIX( rabbit_object_total );
}

VALUE rabbit_gc_work_count( VALUE self )
{
	return INT2FIX( GC_work_count );
}

VALUE rabbit_gc_disable( VALUE self )
{
	GC_STOP = 1;
	return R_NULL;
}

void Heap_Setup()
{
	rabbit_heap_stretch();
}

void Init_Stack( VALUE *ptr )
{
	VALUE start;
	
	if( !ptr ) {
		ptr = &start;
	}
	rabbit_gc_stack_start = ptr;
	
}

void Gc_Setup()
{
	C_Gc = rabbit_define_class( "GC", C_Object );

	rabbit_define_singleton_method( C_Gc, "start", rabbit_gc_start, 0 );
	rabbit_define_singleton_method( C_Gc, "work_count", rabbit_gc_work_count, 0 );
	rabbit_define_singleton_method( C_Gc, "alloc_total", rabbit_gc_alloc_total, 0 );
	rabbit_define_singleton_method( C_Gc, "peak_object", rabbit_gc_object_total, 0 );
	rabbit_define_singleton_method( C_Gc, "disable", rabbit_gc_disable, 0 );
}

