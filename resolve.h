#ifndef _RESOLVE_H
#define _RESOLVE_H

#ifdef WIN32
	typedef int socklen_t;
	struct sockaddr;
#else
	#include <sys/socket.h>
#endif

// try to parse hostname
//  * as dot notation (1.1.1.1)
//  * as ipv6 notation (abcd:ef00::1)
//  * as hostname (resolve DNS)
// returns 1 on success
// family can be used to force ipv4 or ipv6. Use AF_UNSPEC (allow both), AF_INET or AF_INET6 to force ipv4 or ipv6
int resolveName(struct sockaddr* addr, socklen_t* addr_len, const char* hostname, int family);

#endif
