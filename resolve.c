#include "resolve.h"

#include "error_handling.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <ws2tcpip.h>
#else // !WIN32
#include <arpa/inet.h> // inet_pton (parsing ipv4 and ipv6 notation)

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

int resolveName( struct sockaddr *addr, socklen_t *addr_len, const char *hostname, int family )
{
	int result;
	struct addrinfo *res = NULL;

	// zero addr
	memset( addr, 0, *addr_len );

	// try to parse ipv4
	if ( family == AF_UNSPEC || family == AF_INET )
	{
		result = inet_pton( AF_INET, hostname, &( (struct sockaddr_in *)addr )->sin_addr );
		if ( result == 1 )
		{
			addr->sa_family = AF_INET;
			*addr_len = sizeof( struct sockaddr_in );
			return 1;
		}
	}

	// try to parse ipv6
	if ( family == AF_UNSPEC || family == AF_INET6 )
	{
		result = inet_pton( AF_INET6, hostname, &( (struct sockaddr_in6 *)addr )->sin6_addr );
		if ( result == 1 )
		{
			addr->sa_family = AF_INET6;
			*addr_len = sizeof( struct sockaddr_in6 );
			return 1;
		}
	}

	// try to resolve DNS
	struct addrinfo hints;
	memset( &hints, 0, sizeof( struct addrinfo ) );
	hints.ai_family = family; // AF_UNSPEC, AF_INET, AF_INET6
	hints.ai_socktype = 0; // 0 = any, SOCK_STREAM, SOCK_DGRAM
	hints.ai_protocol = 0; // 0 = any
	hints.ai_flags = 0; // no flags

	result = getaddrinfo( hostname, NULL, &hints, &res );
	if ( result != 0 )
	{
		ERRM( "Error: cannot resolve hostname %s: %s\n", hostname, gai_strerror( result ) );
		exit( -1 );
	}

	if ( res->ai_addrlen > *addr_len )
	{
		// error - this should not happen
		ERRM( "Error addr is to short. required length: %d, available length: %d", res->ai_addrlen,
			*addr_len );
		freeaddrinfo( res );
		exit( -1 );
	}
	memcpy( addr, res->ai_addr, res->ai_addrlen );
	*addr_len = res->ai_addrlen;

	freeaddrinfo( res );
	return 1;
}
