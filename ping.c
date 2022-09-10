//Copyright 2017 <>< C. Lohr, under the MIT/x11 License
//Rewritten from Sean Walton and Macmillan Publishers.
//Most of it was rewritten but the header was never updated.
//Now I finished the job.

#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "ping.h"
#include "error_handling.h"

#ifdef TCC
#include "tccheader.h"
#endif

int ping_failed_to_send;
float pingperiodseconds;
int precise_ping;
struct sockaddr_in6 psaddr;
socklen_t psaddr_len;
int af_family;

#ifdef WIN_USE_NO_ADMIN_PING

#ifdef TCC
	//...
#else
#include <inaddr.h>
#include <ws2tcpip.h>
#include <ipexport.h>
#include <icmpapi.h>
#include <windows.h>
#endif

#include "rawdraw/os_generic.h"



#define MAX_PING_SIZE 16384
#define PINGTHREADS 100

#if !defined( MINGW_BUILD ) && !defined( TCC )
	#pragma comment(lib, "Ws2_32.lib")
	#pragma comment(lib, "iphlpapi.lib")
#endif

static og_sema_t s_disp;
static og_sema_t s_ping;

void ping_setup(const char * device)
{

	s_disp = OGCreateSema();
	s_ping = OGCreateSema();
	//This function is executed first.
}

void listener()
{
	static uint8_t listth;
	if( listth ) return;
	listth = 1;

	OGUnlockSema( s_disp );
	//Normally needs to call display(buf + 28, bytes - 28 ); on successful ping.
	//This function is executed as a thread after setup.
	//Really, we just use the s_disp semaphore to make sure we only launch disp's at correct times.

	while(1) { OGSleep( 100000 ); }
	return;
}

static HANDLE pinghandles[PINGTHREADS];

static void * pingerthread( void * v )
{
	uint8_t ping_payload[MAX_PING_SIZE];

	HANDLE ih = *((HANDLE*)v);

	int timeout_ms = pingperiodseconds * (PINGTHREADS-1) * 1000;
	while(1)
	{
		OGLockSema( s_ping );
		int rl = load_ping_packet( ping_payload, sizeof( ping_payload ) );
		struct repl_t
		{
			ICMP_ECHO_REPLY rply;
			uint8_t err_data[16384];
		} repl;

		DWORD res = IcmpSendEcho( ih,
			psaddr.sin_addr.s_addr, ping_payload, rl,
			0, &repl, sizeof( repl ),
			timeout_ms );
		int err;
		if( !res ) err = GetLastError();
		OGLockSema( s_disp );

		if( !res )
		{
			if( err == 11050 )
			{
				printf( "GENERAL FAILURE\n" );
			}
			else
			{
				printf( "ERROR: %d\n", err );
			}
		}
		if( res )
		{
			display( repl.rply.Data, rl );
		}
		OGUnlockSema( s_disp );
	}
	return 0;
}

void ping(struct sockaddr_in *addr )
{
	int i;
	//Launch pinger threads
	for( i = 0; i < PINGTHREADS; i++ )
	{
		HANDLE ih = pinghandles[i] = IcmpCreateFile();
		if( ih == INVALID_HANDLE_VALUE )
		{
			ERRM( "Cannot create ICMP thread %d\n", i );
			exit( 0 );
		}

		OGCreateThread( pingerthread, &pinghandles[i] );
	}
	//This function is executed as a thread after setup.

	while(1)
	{
		if( i >= PINGTHREADS-1 ) i = 0;
		else i++;
		OGUnlockSema( s_ping );
		OGUSleep( (int)(pingperiodseconds * 1000000) );
	}
}



#else // ! WIN_USE_NO_ADMIN_PING

//The normal way to do it - only problem is Windows needs admin privs.


#ifdef WIN32
	#include <winsock2.h>
	#define SOL_IP		0
	#define F_SETFL		4
	#define ICMP_ECHO	8
	#define IP_TTL		2
	#define O_NONBLOCK   04000
#ifndef MINGW_BUILD
	#pragma comment(lib, "Ws2_32.lib")
#endif
	#include <windows.h>
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <stdint.h>
#else // ! WIN32
	#ifdef __FreeBSD__
		#include <netinet/in.h>
	#endif
	#include <unistd.h>
	#include <sys/socket.h>
	#include <resolv.h>
	#include <netdb.h>
	#if defined(__APPLE__) || defined(__FreeBSD__)
		#ifndef SOL_IP
			#define SOL_IP IPPROTO_IP
		#endif
	#endif
	#include <netinet/ip.h>
	#include <netinet/ip_icmp.h>
	#include <netinet/icmp6.h>
	#include <arpa/inet.h> // inet_pton (parsing ipv4 and ipv6 notation)
#endif

#include "rawdraw/os_generic.h"

#if defined WIN32 || defined __APPLE__
struct icmphdr
{
	uint8_t		type;
	uint8_t		code;
	uint16_t	checksum;
	union
	{
		struct
		{
			uint16_t	id;
			uint16_t	sequence;
		} echo;
		uint32_t	gateway;
		struct
		{
			uint16_t	__unused;
			uint16_t	mtu;
		} frag;
	} un;
};
#endif


#define PACKETSIZE	65536

struct packet
{
#ifdef __FreeBSD__
	struct icmp hdr;
	unsigned char msg[PACKETSIZE-sizeof(struct icmp)];
#else
	struct icmphdr hdr;
	unsigned char msg[PACKETSIZE-sizeof(struct icmphdr)];
#endif
};

int sd;
int pid=-1;

uint16_t checksum( const unsigned char * start, uint16_t len )
{
	uint16_t i;
	const uint16_t * wptr = (uint16_t*) start;
	uint32_t csum = 0;
	for (i=1;i<len;i+=2)
		csum += (uint32_t)(*(wptr++));
	if( len & 1 )  //See if there's an odd number of bytes?
		csum += *(uint8_t*)wptr;
	if (csum>>16)
		csum = (csum & 0xFFFF)+(csum >> 16);
	//csum = (csum>>8) | ((csum&0xff)<<8);
	return ~csum;
}

// setsockopt TTL to 255
void setTTL(int sock)
{
	const int val=255;

	assert(af_family == AF_INET || af_family == AF_INET6);

	if ( setsockopt(sd, (af_family == AF_INET) ? SOL_IP : SOL_IPV6, IP_TTL, &val, sizeof(val)) != 0)
	{
		ERRM("Error: Failed to set TTL option.  Are you root?  Or can do sock_raw sockets?\n");
		exit( -1 );
	}
}

// 0 = failed, 1 = this is a ICMP Response
int isICMPResponse(unsigned char* buf, int bytes)
{
	assert(af_family == AF_INET || af_family == AF_INET6);

	if( bytes == -1 ) return 0;

	if( af_family == AF_INET ) // ipv4 compare
	{
		if( buf[9] != IPPROTO_ICMP ) return 0;
		if( buf[20] !=  ICMP_ECHOREPLY ) return 0;

	}
	else if( af_family == AF_INET6 ) // ipv6 compare
	{
		if( buf[0] != ICMP6_ECHO_REPLY ) {
			printf("buf[0] failed\n");
			return 0;
		}
	}

	return 1;
}

int createSocket()
{
	if( af_family == AF_INET )
	{
		return socket(PF_INET, SOCK_RAW, IPPROTO_ICMP);
	}
	else if( af_family == AF_INET6 )
	{
		return socket(PF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
	}

	// invalid af_family
	return -1;
}

// try to parse hostname
//  * as dot notation (1.1.1.1)
//  * as ipv6 notation (abcd:ef00::1)
//  * as hostname (resolve DNS)
static int resolveName(struct sockaddr* addr, socklen_t* addr_len, const char* hostname)
{
	// try to parse ipv4
	int parseresult = inet_pton(AF_INET, hostname, &((struct sockaddr_in*) addr)->sin_addr);
	if(parseresult == 1)
	{
		struct sockaddr_in* ipv4addr = ((struct sockaddr_in*) addr);
		ipv4addr->sin_family = AF_INET;
		*addr_len = sizeof(struct sockaddr_in);
		return 1;
	}

	// try to parse ipv6
	parseresult = inet_pton(AF_INET6, hostname, &((struct sockaddr_in6*) addr)->sin6_addr);
	if(parseresult == 1)
	{
		struct sockaddr_in6* ipv6addr = ((struct sockaddr_in6*) addr);
		ipv6addr->sin6_family = AF_INET6;
		*addr_len = sizeof(struct sockaddr_in6);
		return 1;
	}

	// try to resolve DNS
	struct addrinfo* res = NULL;
	int errorcode = getaddrinfo(hostname, NULL, NULL, &res);

	if( errorcode != 0)
	{
		ERRM("Error: cannot resolve hostname %s: %s\n", hostname, gai_strerror(errorcode));
		exit( -1 );
	}

	if(res->ai_addrlen > *addr_len)
	{
		// error
		exit( -1 );
	}
	memcpy(addr, res->ai_addr, res->ai_addrlen);
	*addr_len = res->ai_addrlen;

	freeaddrinfo(res);
	return 1;
}

void listener()
{
#ifndef WIN32
	int sd = createSocket();

	setTTL(sd);
#endif

	struct sockaddr_in6 addr;
	unsigned char buf[66000];
#ifdef WIN32
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#endif
	for (;;)
	{
		socklen_t addrlenval=sizeof(addr);
		int bytes;

#ifdef WIN32
		WSAPOLLFD fda[1];
		fda[0].fd = sd;
		fda[0].events = POLLIN;
		WSAPoll(fda, 1, 10);
#endif
		keep_retry_quick:

		bytes = recvfrom(sd, buf, sizeof(buf), 0, (struct sockaddr*)&addr, &addrlenval );
		if( !isICMPResponse(buf, bytes) ) continue;

		// compare the sender
		if( memcmp(&addr, &psaddr, addrlenval) != 0 ) continue;

		// sizeof(packet.hdr) + 20
		int offset = 0;
		if(af_family == AF_INET) // ipv4
		{
#ifdef __FreeBSD__
			offset = 48;
#else
			offset = 28;
#endif
		}
		else // ipv6
		{
			offset = 8;
		}

		if ( bytes > 0 )
			display(buf + offset, bytes - offset );
		else
		{
			ERRM("Error: recvfrom failed");
		}

		goto keep_retry_quick;
	}

	ERRM( "Fault on listen().\n" );
	exit( 0 );
}

void ping(struct sockaddr *addr, socklen_t addr_len )
{
	int cnt=1;

#ifdef WIN32
	{
		//Setup windows socket for nonblocking io.
		unsigned long iMode = 1;
		ioctlsocket(sd, FIONBIO, &iMode);
	}
#else
	if ( fcntl(sd, F_SETFL, O_NONBLOCK) != 0 )
		ERRM("Warning: Request nonblocking I/O failed.");
#endif

	double stime = OGGetAbsoluteTime();

	struct packet pckt;
	do
	{
		int rsize = load_ping_packet( pckt.msg, sizeof( pckt.msg ) );
		memset( &pckt.hdr, 0, sizeof( pckt.hdr ) ); //This needs to be here, but I don't know why, since I think the struct is fully populated.
#ifdef __FreeBSD__
		pckt.hdr.icmp_code = 0;
		pckt.hdr.icmp_type = ICMP_ECHO;
		pckt.hdr.icmp_id = pid;
		pckt.hdr.icmp_seq = cnt++;
		pckt.hdr.icmp_cksum = checksum((const unsigned char *)&pckt, sizeof( pckt.hdr ) + rsize );
#else
		pckt.hdr.code = 0;
		pckt.hdr.type = (af_family == AF_INET) ? ICMP_ECHO : ICMP6_ECHO_REQUEST;
		pckt.hdr.un.echo.id = pid;
		pckt.hdr.un.echo.sequence = cnt++;
		pckt.hdr.checksum = checksum((const unsigned char *)&pckt, sizeof( pckt.hdr ) + rsize );
#endif

		int sr = sendto(sd, (char*)&pckt, sizeof( pckt.hdr ) + rsize , 0, addr, addr_len);

		if( sr <= 0 )
		{
			ping_failed_to_send = 1;
			ERRM("Ping send failed: %s familiy: %d\n", strerror(errno), addr->sa_family);
		}
		else
		{
			ping_failed_to_send = 0;
		}

		if( precise_ping )
		{
			double ctime;
			do
			{
				ctime = OGGetAbsoluteTime();
				if( pingperiodseconds >= 1000 ) stime = ctime;
			} while( ctime < stime + pingperiodseconds );
			stime += pingperiodseconds;
		}
		else
		{
			if( pingperiodseconds > 0 )
			{
				uint32_t dlw = 1000000.0*pingperiodseconds;
				OGUSleep( dlw );
			}
		}
	} 	while( pingperiodseconds >= 0 );
	//close( sd ); //Hacky, we don't close here because SD doesn't come from here, rather  from ping_setup.  We may want to run this multiple times.
}

void ping_setup(const char * strhost, const char * device)
{
	pid = getpid();

#ifdef WIN32
	WSADATA wsaData;
	int r =	WSAStartup(MAKEWORD(2,2), &wsaData);
	if( r )
	{
		ERRM( "Fault in WSAStartup\n" );
		exit( -2 );
	}
#endif


#ifdef WIN32
	sd = WSASocket(AF_INET, SOCK_RAW, IPPROTO_ICMP, 0, 0, WSA_FLAG_OVERLAPPED);
	{
		int lttl = 0xff;
		if (setsockopt(sd, IPPROTO_IP, IP_TTL, (const char*)&lttl, sizeof(lttl)) == SOCKET_ERROR)
		{
			printf( "Warning: No IP_TTL.\n" );
		}
	}
#else
	// resolve host
	memset(&psaddr, 0, sizeof(psaddr));
	psaddr_len = sizeof(struct sockaddr_in6);
	resolveName((struct sockaddr*) &psaddr, &psaddr_len, strhost);
	af_family = psaddr.sin6_family;

	sd = createSocket();

	setTTL(sd);

	if(device)
	{
		if( setsockopt(sd, SOL_SOCKET, SO_BINDTODEVICE, device, strlen(device) +1) != 0)
		{
			ERRM("Error: Failed to set Device option.  Are you root?  Or can do sock_raw sockets?\n");
			exit( -1 );
		}
	}

#endif
	if ( sd < 0 )
	{
		ERRM("Error: Could not create raw socket\n");
		exit(0);
	}

}

#endif // WIN_USE_NO_ADMIN_PING


void do_pinger( )
{
	ping((struct sockaddr*) &psaddr, psaddr_len );
}


char errbuffer[1024];

