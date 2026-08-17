#include "pinghostlist.h"

#include <stddef.h>
#include <stdlib.h>

void appendPingHost( struct PingHost ** list, unsigned int * listSize, const char * newEntryValue )
{
	// find last entry
	struct PingHost * current = *list;
	struct PingHost * last = current;
	while( current )
	{
		struct PingHost * next = current->next;
		last = current;
		current = next;
	}

	struct PingHost* newEntry = malloc( sizeof(struct PingHost) );
	newEntry->host = newEntryValue;
	newEntry->next = NULL;

	if ( last )
	{
		last->next = newEntry;
	}
	else
	{
		*list = newEntry;
	}

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
