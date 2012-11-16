#include "rabbit.h"
#include "version.h"

char*
rabbit_version_dup()
{
	char version[100];
	sprintf( version, "rabbit %s (build %s)", RABBIT_VERSION, RABBIT_BUILD_DATE );
	return strdup( version );
}

VALUE
rabbit_f_get_version( VALUE self )
{
	VALUE result;
	char* version = rabbit_version_dup();
	
	result = rabbit_string_new( strlen(version), version );
	free( version );

	return result;
}

