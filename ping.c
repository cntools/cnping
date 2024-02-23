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
#include "resolve.h"

#ifdef TCC
#include "tccheader.h"
#endif

float pingperiodseconds;
int precise_ping;
int using_regular_ping;

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

// used to pack arguments for windows ping threads
struct WindowsPingArgs
{
	struct PreparedPing* pp;
	HANDLE icmpHandle;
};


#define MAX_PING_SIZE 16384
#define PINGTHREADS 100

#if !defined( MINGW_BUILD ) && !defined( TCC )
	#pragma comment(lib, "Ws2_32.lib")
	#pragma comment(lib, "iphlpapi.lib")
#endif

struct PreparedPing* ping_setup(const char * strhost, const char * device)
{
	struct PreparedPing* pp = (struct PreparedPing*) malloc(sizeof(struct PreparedPing));

	memset(&pp->psaddr, 0, sizeof(pp->psaddr));
	pp->psaddr_len = sizeof(pp->psaddr);

	// resolve host
	if( strhost )
		resolveName((struct sockaddr*) &pp->psaddr, &pp->psaddr_len, strhost, AF_INET); // only resolve ipv4 on windows
	else
		pp->psaddr.sin6_family = AF_INET;

	pp->s_disp = OGCreateSema();
	pp->s_ping = OGCreateSema();
	//This function is executed first.

	return pp;
}

void listener( struct PreparedPing* pp )
{
	OGUnlockSema( pp->s_disp );
	//Normally needs to call display(buf + 28, bytes - 28 ); on successful ping.
	//This function is executed as a thread after setup.
	//Really, we just use the s_disp semaphore to make sure we only launch disp's at correct times.

	while(1) { OGSleep( 100000 ); }
	return;
}

static void * pingerthread( void * v )
{
	uint8_t ping_payload[MAX_PING_SIZE];

	// copy arguments and free struct
	struct WindowsPingArgs* args = (struct WindowsPingArgs*) v;
	struct PreparedPing* pp = args->pp;
	HANDLE ih = args->icmpHandle;
	free(args);

	int timeout_ms = pingperiodseconds * (PINGTHREADS-1) * 1000;
	while(1)
	{
		OGLockSema( pp->s_ping );
		int rl = load_ping_packet( ping_payload, sizeof( ping_payload ), PingData + pp->pingHostId );
		struct repl_t
		{
			ICMP_ECHO_REPLY rply;
			uint8_t err_data[16384];
		} repl;

		DWORD res = IcmpSendEcho( ih,
			((struct sockaddr_in*) &pp->psaddr)->sin_addr.s_addr, ping_payload, rl,
			0, &repl, sizeof( repl ),
			timeout_ms );
		int err;
		if( !res ) err = GetLastError();
		OGLockSema( pp->s_disp );

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
			display( repl.rply.Data, rl, pp->pingHostId );
		}
		OGUnlockSema( pp->s_disp );
	}
	return 0;
}

// create PINGTHREADS amount of pingerthread
void createThreads( struct PreparedPing* pp )
{
	for( int i = 0; i < PINGTHREADS; i++ )
	{
		HANDLE ih = IcmpCreateFile();
		if( ih == INVALID_HANDLE_VALUE )
		{
			ERRM( "Cannot create ICMP thread %d\n", i );
			exit( 0 );
		}

		struct WindowsPingArgs* args = (struct WindowsPingArgs*) malloc(sizeof(struct WindowsPingArgs));
		args->pp = pp;
		args->icmpHandle = ih;
		OGCreateThread( pingerthread, args );
	}
}

// ipv6 ping is not supported on windows -> exit if ipv6 is required
void checkIPv6( int family )
{
	if( family != AF_INET )
	{
		// ipv6 ICMP Ping is not implemented on windows - PRs welcome
		ERRM( "ERROR: ipv6 ICMP Ping is not supported on windows\n" );
		exit( -1 );
	}
}

void singleping( struct PreparedPing* pp )
{
	checkIPv6( pp->psaddr.sin6_family );

	static int did_init_threads = 0;
	if( !did_init_threads )
	{
		did_init_threads = 1;
		//Launch pinger threads
		createThreads( pp );
	}
	//This function is executed as a thread after setup.

	OGUnlockSema( pp->s_ping );
}

void ping( struct PreparedPing* pp )
{
	using_regular_ping = 1;

	checkIPv6( pp->psaddr.sin6_family );

	//Launch pinger threads
	createThreads( pp );

	//This function is executed as a thread after setup.

	while(1)
	{
		OGUnlockSema( pp->s_ping );
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

int pid=-1;

int min( int a, int b )
{
	return a < b ? a : b;
}

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


#ifdef WIN32
void setTTL( int sock, int family )
{
	(void) sock;
	(void) family;
	// nop on win
}

#else
// setsockopt TTL to 255
void setTTL( int sock, int family )
{
	static const int val=255;

	assert(family == AF_INET || family == AF_INET6);

	if ( setsockopt( sock, (family == AF_INET) ? SOL_IP : SOL_IPV6, IP_TTL, &val, sizeof(val)) != 0)
	{
		ERRM("Error: Failed to set TTL option.  Are you root?  Or can do sock_raw sockets?\n");
		exit( -1 );
	}
}
#endif

void setNoBlock( int sock )
{
#ifdef WIN32
	{
		//Setup windows socket for nonblocking io.
		unsigned long iMode = 1;
		ioctlsocket(sock, FIONBIO, &iMode);
	}
#else
	if ( fcntl(sock, F_SETFL, O_NONBLOCK) != 0 )
		ERRM("Warning: Request nonblocking I/O failed.");
#endif
}

// returns 1 = success; 0 = failed
int bindDevice( int sock, const char* device )
{
#ifdef WIN32
	(void) sock;
	(void) device;

#else
	if(device)
	{
		if( setsockopt( sock, SOL_SOCKET, SO_BINDTODEVICE, device, strlen(device) +1 ) != 0 )
		{
			return 0;
		}
	}
#endif
	return 1;
}

// 0 = failed, 1 = this is a ICMP Response
int isICMPResponse( int family, unsigned char* buf, int bytes )
{
	if( family != AF_INET && family != AF_INET6 ) return 0;

	if( bytes < 1 ) return 0;

	if( family == AF_INET ) // ipv4 compare
	{
		if( bytes < 21 ) return 0;
		if( buf[9] != IPPROTO_ICMP ) return 0;
		if( buf[20] !=  ICMP_ECHOREPLY ) return 0;

	}
	else if( family == AF_INET6 ) // ipv6 compare
	{
		if( buf[0] != ICMP6_ECHO_REPLY ) return 0;
	}

	return 1;
}

int createSocket( int family )
{
#ifdef WIN32
	// no ipv6 support for windows
	int fd = WSASocket(AF_INET, SOCK_RAW, IPPROTO_ICMP, 0, 0, WSA_FLAG_OVERLAPPED);
	{
		static const int lttl = 0xff;
		if (setsockopt(fd, IPPROTO_IP, IP_TTL, (const char*)&lttl, sizeof(lttl)) == SOCKET_ERROR)
		{
			printf( "Warning: No IP_TTL.\n" );
		}
	}
	return fd;
#else
	if( family == AF_INET )
	{
		return socket(PF_INET, SOCK_RAW, IPPROTO_ICMP);
	}
	else if( family == AF_INET6 )
	{
		return socket(PF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
	}

	// invalid af_family
	return -1;
#endif
}

void listener( struct PreparedPing* pp )
{
	int listenSock = createSocket( pp->psaddr.sin6_family );

#ifndef WIN32
	setTTL( listenSock, pp->psaddr.sin6_family );
#endif

	struct sockaddr_in6 recvFromAddr;
	unsigned char buf[66000];
#ifdef WIN32
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#endif
	for (;;)
	{
		socklen_t recvFromAddrLen = sizeof(recvFromAddr);

#ifdef WIN32
		WSAPOLLFD fda[1];
		fda[0].fd = listenSock;
		fda[0].events = POLLIN;
		WSAPoll(fda, 1, 10);
#endif

		int bytes;

keep_retry_quick:
		bytes = recvfrom( listenSock, (void*) buf, sizeof(buf), 0, (struct sockaddr*)&recvFromAddr, &recvFromAddrLen );
		if( !isICMPResponse( recvFromAddr.sin6_family, buf, bytes) ) continue;

		// compare the sender
		if( using_regular_ping && memcmp(&recvFromAddr, &pp->psaddr, min(recvFromAddrLen, pp->psaddr_len) ) != 0 ) continue;

		// sizeof(packet.hdr) + 20
		int offset = 0;
		if( recvFromAddr.sin6_family == AF_INET ) // ipv4
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
			display(buf + offset, bytes - offset, pp->pingHostId );
		else
		{
			ERRM("Error: recvfrom failed");
		}

		goto keep_retry_quick;
	}

	ERRM( "Fault on listen().\n" );
	exit( 0 );
}

// fill a packet with data ready to be written to a raw socket
// returns the size of the packet
int constructPack(struct packet* pckt, int family, int pingHostId, int cnt)
{
	int rsize = load_ping_packet( pckt->msg, sizeof( pckt->msg ), PingData + pingHostId );

	// add the size of the header
	rsize += sizeof( pckt->hdr );

	// checksum is calculated, with zerofilled checksum field
	pckt->hdr.checksum = 0;

	const uint8_t icmp_type = (family == AF_INET) ? ICMP_ECHO : ICMP6_ECHO_REQUEST;
#ifdef __FreeBSD__
	pckt->hdr.icmp_code = 0;
	pckt->hdr.icmp_type = icmp_type;
	pckt->hdr.icmp_id = pid;
	pckt->hdr.icmp_seq = cnt;
	pckt->hdr.icmp_cksum = checksum((const unsigned char *) pckt, rsize);
#else
	pckt->hdr.code = 0;
	pckt->hdr.type = icmp_type;
	pckt->hdr.un.echo.id = pid;
	pckt->hdr.un.echo.sequence = cnt;
	pckt->hdr.checksum = checksum((const unsigned char *) pckt, rsize);
#endif
	return rsize;
}

void sendOnePing( struct PreparedPing* pp , int count )
{
	struct packet pckt;

	int rsize = constructPack( &pckt, pp->psaddr.sin6_family, pp->pingHostId, count );
	int sr = sendto(pp->fd, (char*)&pckt, rsize , 0, (const struct sockaddr*) &pp->psaddr, pp->psaddr_len);

	struct PingData* pd = PingData + pp->pingHostId;
	if( sr <= 0 )
	{
		pd->ping_failed_to_send = 1;
		if( using_regular_ping )
		{
			ERRMB("Ping send failed:\n%s (%d)\n", strerror(errno), errno);
		}
	}
	else
	{
		pd->ping_failed_to_send = 0;
	}
}

void singleping( struct PreparedPing* pp )
{
	setNoBlock( pp->fd );

	sendOnePing( pp, 1 );
}

void ping( struct PreparedPing* pp )
{
	int cnt=1;

	using_regular_ping = 1;
	setNoBlock( pp->fd );

	double stime = OGGetAbsoluteTime();

	do
	{
		sendOnePing( pp, cnt++ );

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
	} while( pingperiodseconds >= 0 );
	//close( sd ); //Hacky, we don't close here because SD doesn't come from here, rather from ping_setup.  We may want to run this multiple times.
}

struct PreparedPing* ping_setup(const char * strhost, const char * device)
{
	pid = getpid();

	struct PreparedPing* pp = (struct PreparedPing*) malloc(sizeof(struct PreparedPing));

	memset(&pp->psaddr, 0, sizeof(pp->psaddr));
	pp->psaddr_len = sizeof(pp->psaddr);
	pp->psaddr.sin6_family = AF_INET;

	// resolve host
	if( strhost )
	{
		resolveName((struct sockaddr*) &pp->psaddr, &pp->psaddr_len, strhost, AF_UNSPEC);
	}

	pp->fd = createSocket( pp->psaddr.sin6_family );
	setTTL(pp->fd, pp->psaddr.sin6_family);

	if ( pp->fd < 0 )
	{
		ERRM("Error: Could not create raw socket\n");
		free(pp);
		exit( -2 );
	}

	if( 0 == bindDevice( pp->fd, device ) )
	{
		ERRM("Error: Failed to set Device option. Are you root? Or can do sock_raw sockets?\n");
		free( pp );
		exit( -1 );
	}

	return pp;
}

#endif // WIN_USE_NO_ADMIN_PING

// used by the ERRMB makro from error_handling.h
char errbuffer[1024];

