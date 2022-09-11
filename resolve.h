#ifndef _RESOLVE_H
#define _RESOLVE_H

#ifdef WIN32
	typedef int socklen_t;
#else
	#include <sys/socket.h>
#endif

// try to parse hostname
//  * as dot notation (1.1.1.1)
//  * as ipv6 notation (abcd:ef00::1)
//  * as hostname (resolve DNS)
// returns 1 on success
int resolveName(struct sockaddr* addr, socklen_t* addr_len, const char* hostname);

#endif
