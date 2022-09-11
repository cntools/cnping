#include "resolve.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error_handling.h"

#ifdef WIN32
	#include <ws2tcpip.h>
#else // !WIN32
	#include <arpa/inet.h> // inet_pton (parsing ipv4 and ipv6 notation)

	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netdb.h>
#endif

int resolveName(struct sockaddr* addr, socklen_t* addr_len, const char* hostname)
{
	int result;
	struct addrinfo* res = NULL;

	// zero addr
	memset(addr, 0, *addr_len);

	// try to parse ipv4
	result = inet_pton(AF_INET, hostname, &((struct sockaddr_in*) addr)->sin_addr);
	if(result == 1)
	{
		addr->sa_family = AF_INET;
		*addr_len = sizeof(struct sockaddr_in);
		return 1;
	}

	// try to parse ipv6
	result = inet_pton(AF_INET6, hostname, &((struct sockaddr_in6*) addr)->sin6_addr);
	if(result == 1)
	{
		addr->sa_family = AF_INET6;
		*addr_len = sizeof(struct sockaddr_in6);
		return 1;
	}

	// try to resolve DNS
	result = getaddrinfo(hostname, NULL, NULL, &res);
	if( result != 0)
	{
		ERRM( "Error: cannot resolve hostname %s: %s\n", hostname, gai_strerror(result) );
		exit( -1 );
	}

	if(res->ai_addrlen > *addr_len)
	{
		// error - this should not happen
		freeaddrinfo(res);
		exit( -1 );
	}
	memcpy(addr, res->ai_addr, res->ai_addrlen);
	*addr_len = res->ai_addrlen;

	freeaddrinfo(res);
	return 1;
}
