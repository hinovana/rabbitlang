#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "rabbit.h"
#include "st.h"

struct st_table* rabbit_symbol_tbl;
struct st_table* rabbit_recv_symbol_tbl;

// 連番
static TAG rabbit_tag_serial = 0x1;

// debug
void bin_dump( TAG tag, char* symbol )
{
	fprintf( stderr, "0x%08x\t%s\n", (unsigned int)tag, symbol );
}

/**
 *  SYMBOL FLAG(32bit)
 *  MSB --------------------------- LSB
 *  NNNNNNNN NNNNNNNN NNNNNNNN 00000000 -- symbol id
 *  NNNNNNNN NNNNNNNN NNNNNNNN 0000000F -- Global variable flag
 *  NNNNNNNN NNNNNNNN NNNNNNNN 000000F0 -- Instance variable flag
 */
TAG rabbit_name2tag( const char* name )
{
	TAG tag;
	
	assert( name );
	
	if( st_lookup( rabbit_symbol_tbl, (st_data_t)name, &tag ) != 0 ) {
		return tag;
	}
	tag = rabbit_tag_serial++;
	
	// 下位8ビットはフラグエリアだよってことで
	tag <<= 8;
	
	switch( name[0] ) {
	case '$':
		tag |= TAG_FLAG_GLOBAL;
		break;
	case '@':
		tag |= TAG_FLAG_IVAL;
		break;
	}

	// mallocは関数内でしてますよ
	name = strdup(name);

	st_add_direct( rabbit_symbol_tbl, (st_data_t)name, tag );
	st_add_direct( rabbit_recv_symbol_tbl, tag, (st_data_t)name );

	return tag;
}

char* rabbit_tag2name( TAG tag )
{
	st_data_t name;
	
	if( st_lookup( rabbit_recv_symbol_tbl, tag, &name ) ) {
		return (char*)name;
	}
	abort();
}

void Symbol_Setup()
{
	rabbit_symbol_tbl = st_init_strtable_with_size(200);
	rabbit_recv_symbol_tbl = st_init_numtable_with_size(200);
}

