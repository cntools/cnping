#ifndef PING_HOST_LIST_H
#define PING_HOST_LIST_H


// linked list of hosts to ping
struct PingHost
{
	const char * host;
	struct PingHost* next;
};

// add a new entry to the list
void prependPingHost( struct PingHost ** list, unsigned int * listSize, const char * newEntryValue );

// delete the list
// *list and *listsize will be set to 0
void freePingHostList( struct PingHost ** list, unsigned int * listSize );

#endif
