#include "pinghostlist.h"

#include <stddef.h>
#include <stdlib.h>

void prependPingHost( struct PingHost ** list, unsigned int * listSize, const char * newEntryValue )
{
	struct PingHost* newEntry = malloc( sizeof(struct PingHost) );
	newEntry->host = newEntryValue;
	newEntry->next = *list;
	*list = newEntry;
	(*listSize) ++;
}

void freePingHostList( struct PingHost ** list, unsigned int * listSize )
{
	struct PingHost * current = *list;
	while( current )
	{
		struct PingHost * next = current->next;
		free( current );
		current = next;
	}
	*list = NULL;
	if(listSize)
	{
		*listSize = 0;
	}
}
