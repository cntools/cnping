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

int ping_failed_to_send;
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

static og_sema_t s_disp;
static og_sema_t s_ping;

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

	s_disp = OGCreateSema();
	s_ping = OGCreateSema();
	//This function is executed first.

	return pp;
}

void listener( struct PreparedPing* pp )
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
		OGLockSema( s_ping );
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
			display( repl.rply.Data, rl, pp->pingHostId );
		}
		OGUnlockSema( s_disp );
	}
	return 0;
}

void singleping( struct PreparedPing* pp )
{
	int i;

	if( pp->psaddr.sin6_family != AF_INET )
	{
		// ipv6 ICMP Ping is not supported on windows
		ERRM( "ERROR: ipv6 ICMP Ping is not supported on windows\n" );
		exit( -1 );
	}

	static int did_init_threads = 0;
	if( !did_init_threads )
	{
		did_init_threads = 1;
		//Launch pinger threads
		for( i = 0; i < PINGTHREADS; i++ )
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
	//This function is executed as a thread after setup.

	if( i >= PINGTHREADS-1 ) i = 0;
	else i++;
	OGUnlockSema( s_ping );
}

void ping( struct PreparedPing* pp )
{
	int i;

	using_regular_ping = 1;

	if( pp->psaddr.sin6_family != AF_INET )
	{
		// ipv6 ICMP Ping is not supported on windows
		ERRM( "ERROR: ipv6 ICMP Ping is not supported on windows\n" );
		exit( -1 );
	}

	//Launch pinger threads
	for( i = 0; i < PINGTHREADS; i++ )
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


#ifndef WIN32
// setsockopt TTL to 255
void setTTL( int sock, int family )
{
	const int val=255;

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

// 0 = failed, 1 = this is a ICMP Response
int isICMPResponse( int family, unsigned char* buf, int bytes )
{
	assert( family == AF_INET || family == AF_INET6);

	if( bytes == -1 ) return 0;

	if( family == AF_INET ) // ipv4 compare
	{
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
}

void listener( struct PreparedPing* pp )
{
#ifndef WIN32
	int sd = createSocket( pp->psaddr.sin6_family );

	setTTL( sd, pp->psaddr.sin6_family );
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
		fda[0].fd = sd;
		fda[0].events = POLLIN;
		WSAPoll(fda, 1, 10);
#endif

		int bytes;

keep_retry_quick:
		bytes = recvfrom(sd, buf, sizeof(buf), 0, (struct sockaddr*)&recvFromAddr, &recvFromAddrLen );
		if( !isICMPResponse( pp->psaddr.sin6_family, buf, bytes) ) continue;

		// compare the sender
		if( using_regular_ping && memcmp(&recvFromAddr, &pp->psaddr, recvFromAddrLen) != 0 ) continue;

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

int constructPack(struct packet* pckt, int family, int pingHostId, int cnt)
{
	int rsize = load_ping_packet( pckt->msg, sizeof( pckt->msg ), PingData + pingHostId );

	// This needs to be here, but I don't know why, since I think the struct is fully populated.
	memset( &pckt->hdr, 0, sizeof( pckt->hdr ) );

	const uint8_t icmp_type = (family == AF_INET) ? ICMP_ECHO : ICMP6_ECHO_REQUEST;
#ifdef __FreeBSD__
	pckt->hdr.icmp_code = 0;
	pckt->hdr.icmp_type = icmp_type;
	pckt->hdr.icmp_id = pid;
	pckt->hdr.icmp_seq = cnt;
	pckt->hdr.icmp_cksum = checksum((const unsigned char *) &pckt, sizeof( pckt->hdr ) + rsize);
#else
	pckt->hdr.code = 0;
	pckt->hdr.type = icmp_type;
	pckt->hdr.un.echo.id = pid;
	pckt->hdr.un.echo.sequence = cnt;
	pckt->hdr.checksum = checksum((const unsigned char *) &pckt, sizeof( pckt->hdr ) + rsize);
#endif
	return rsize;
}

void singleping( struct PreparedPing* pp )
{
	setNoBlock( pp->fd );

	struct packet pckt;

	{
		int rsize = constructPack(&pckt, pp->psaddr.sin6_family, pp->pingHostId, 1);

		int sr = sendto(pp->fd, (char*)&pckt, sizeof( pckt.hdr ) + rsize , 0, (const struct sockaddr*) &pp->psaddr, pp->psaddr_len);

		if( sr <= 0 )
		{
			ping_failed_to_send = 1;
			if( using_regular_ping )
			{
				ERRMB("Ping send failed:\n%s (%d)\n", strerror(errno), errno);
			}
		}
		else
		{
			ping_failed_to_send = 0;
		}
	}
}

void ping( struct PreparedPing* pp )
{
	int cnt=1;

	using_regular_ping = 1;
	setNoBlock( pp->fd );

	double stime = OGGetAbsoluteTime();

	struct packet pckt;
	do
	{
		int rsize = constructPack(&pckt, pp->psaddr.sin6_family, pp->pingHostId, cnt++);

		int sr = sendto(pp->fd, (char*) &pckt, sizeof( pckt.hdr ) + rsize , 0, (const struct sockaddr*) &pp->psaddr, pp->psaddr_len);

		if( sr <= 0 )
		{
			ping_failed_to_send = 1;
			if( using_regular_ping )
			{
				ERRMB("Ping send failed:\n%s (%d)\n", strerror(errno), errno);
			}
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
	} while( pingperiodseconds >= 0 );
	//close( sd ); //Hacky, we don't close here because SD doesn't come from here, rather from ping_setup.  We may want to run this multiple times.
}

struct PreparedPing* ping_setup(const char * strhost, const char * device)
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

	struct PreparedPing* pp = (struct PreparedPing*) malloc(sizeof(struct PreparedPing));

	memset(&pp->psaddr, 0, sizeof(pp->psaddr));
	pp->psaddr_len = sizeof(pp->psaddr);

#ifdef WIN32
	pp->fd = WSASocket(AF_INET, SOCK_RAW, IPPROTO_ICMP, 0, 0, WSA_FLAG_OVERLAPPED);
	{
		int lttl = 0xff;
		if (setsockopt(pp->fd, IPPROTO_IP, IP_TTL, (const char*)&lttl, sizeof(lttl)) == SOCKET_ERROR)
		{
			printf( "Warning: No IP_TTL.\n" );
		}
	}
#else
	// resolve host
	if( strhost )
	{
		resolveName((struct sockaddr*) &pp->psaddr, &pp->psaddr_len, strhost, AF_UNSPEC);
	}
	else
	{
		pp->psaddr.sin6_family = AF_INET;
	}

	pp->fd = createSocket( pp->psaddr.sin6_family );

	setTTL(pp->fd, pp->psaddr.sin6_family);

	if(device)
	{
		if( setsockopt(pp->fd, SOL_SOCKET, SO_BINDTODEVICE, device, strlen(device) +1) != 0)
		{
			ERRM("Error: Failed to set Device option.  Are you root?  Or can do sock_raw sockets?\n");
			free(pp);
			exit( -1 );
		}
	}

#endif
	if ( pp->fd < 0 )
	{
		ERRM("Error: Could not create raw socket\n");
		free(pp);
		exit(0);
	}

	return pp;
}

#endif // WIN_USE_NO_ADMIN_PING

// used by the ERRMB makro from error_handling.h
char errbuffer[1024];

