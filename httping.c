#include "httping.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "resolve.h"

#ifndef TCC
#include <unistd.h>
#include <sys/types.h>
#else
#include "tccheader.h"
#endif

#if defined( WIN32 ) || defined( WINDOWS )
	#ifdef TCC
	#else
	#include <ws2tcpip.h>
	#endif
	#define SOL_TCP IPPROTO_TCP
#else
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <netdb.h>
#endif

#include "rawdraw/os_generic.h"
#include "error_handling.h"

#ifndef MSG_NOSIGNAL
	#define MSG_NOSIGNAL 0
#endif

#define HTTPTIMEOUT 3.0

//Callbacks (when started/received)
void HTTPingCallbackStart( int seqno );
void HTTPingCallbackGot( int seqno );

//Don't dynamically allocate resources here, since execution may be stopped arbitrarily.
void DoHTTPing( const char * addy, double minperiod, int * seqnoptr, volatile double * timeouttime, int * socketptr, volatile int * getting_host_by_name, const char * device)
{
#if defined(WIN32) || defined(WINDOWS)
	(void) device; // option is not available for windows. Suppress unused warning.

	WSADATA wsaData;
	int r =	WSAStartup(MAKEWORD(2,2), &wsaData);
	if( r )
	{
		ERRM( "Fault in WSAStartup\n" );
		exit( -2 );
	}
#endif
	struct sockaddr_in6 serveraddr;
	socklen_t serveraddr_len;
	int serverresolve;
	int httpsock;
	int addylen = strlen(addy);
	char hostname[addylen+1];
	memcpy( hostname, addy, addylen + 1 );
	char * eportmarker = strchr( hostname, ':' );
	char * eurl = strchr( hostname, '/' );

	int portno = 80;

	if( eportmarker )
	{
		portno = atoi( eportmarker+1 );
		*eportmarker = 0;
	}
	else
	{
		if( eurl )
			*eurl = 0;
	}

	/* gethostbyname: get the server's DNS entry */
	serveraddr_len = sizeof(serveraddr);
	*getting_host_by_name = 1;
	serverresolve = resolveName((struct sockaddr*) &serveraddr, &serveraddr_len, hostname, AF_UNSPEC);
	*getting_host_by_name = 0;

	if (serverresolve != 1) {
		ERRMB("ERROR, no such host as \"%s\"\n", hostname);
		goto fail;
	}

	/* build the server's Internet address */
	serveraddr.sin6_port = htons(portno);

reconnect:
	*socketptr = httpsock = socket(serveraddr.sin6_family, SOCK_STREAM, 0);
	if (httpsock < 0)
	{
		ERRMB( "Error opening socket\n" );
		return;
	}

	int sockVal = 1;
	// using char* for sockVal for windows
	if (setsockopt(httpsock, SOL_TCP, TCP_NODELAY, (char*) &sockVal, 4) != 0)
	{
		ERRM( "Error: Failed to set TCP_NODELAY\n");
		// not a critical error, we can continue
	}

#if !defined( WIN32 ) && !defined( WINDOWS )
	if(device)
	{
		if( setsockopt(httpsock, SOL_SOCKET, SO_BINDTODEVICE, device, strlen(device) +1) != 0)
		{
			ERRM("Error: Failed to set Device option.\n");
			exit( -1 );
		}
	}
#endif // not windows

	/* connect: create a connection with the server */
	if (connect(httpsock, (struct sockaddr*)&serveraddr, serveraddr_len) < 0)
	{
		ERRMB( "%s: ERROR connecting: %s (%d)\n", hostname, strerror(errno), errno );
		goto fail;
	}

#ifdef __APPLE__
	int opt = 1;
	setsockopt(httpsock, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt));
#endif

	errbuffer[0] = 0;

	while( 1 )
	{
		char buf[8192];

		int n = sprintf( buf, "HEAD %s HTTP/1.1\r\nConnection: keep-alive\r\nHost: %s\r\n\r\n", eurl?eurl:"/favicon.ico", hostname );

		(*seqnoptr) ++;
		HTTPingCallbackStart( *seqnoptr );

		int rs = send( httpsock, buf, n, MSG_NOSIGNAL );
		double starttime = *timeouttime = OGGetAbsoluteTime();
		int breakout = 0;
		if( rs != n ) breakout = 1;
		int endstate = 0;
		while( !breakout )
		{
#ifdef WIN32
			n = recv( httpsock, buf, sizeof(buf)-1, 0);
#else
			n = recv( httpsock, buf, sizeof(buf)-1, MSG_PEEK);
			if( n > 0 ) n = read( httpsock, buf, sizeof(buf)-1);
			else if( n == 0 ) break; //FIN received
#endif

			
			if( n < 0 ) return;
			
			int i;
			for( i = 0; i < n; i++ )
			{
				char c = buf[i];
				switch( endstate )
				{
				case 0: if( c == '\r' ) endstate++; break;
				case 1: if( c == '\n' ) endstate++; else endstate = 0; break;
				case 2: if( c == '\r' ) endstate++; else endstate = 0; break;
				case 3: if( c == '\n' && i == n-1) breakout = 1; else endstate = 0; break;
				}
			}
		}
		*timeouttime = OGGetAbsoluteTime();

		HTTPingCallbackGot( *seqnoptr );

		double delay_time = minperiod - (*timeouttime - starttime);
		if( delay_time > 0 )
			usleep( (int)(delay_time * 1000000) );
		if( !breakout ) {
#ifdef WIN32
			closesocket( httpsock );
#else
			close( httpsock );
#endif
			goto reconnect;
		}
	}
fail:
	return;
}


struct HTTPPingLaunch
{
	const char * addy;
	double minperiod;
	const char * device;

	volatile double timeout_time;
	volatile int failed;
	int seqno;
	int socket;
	volatile int getting_host_by_name;
};

static void * DeployPing( void * v )
{
	struct HTTPPingLaunch *hpl = (struct HTTPPingLaunch*)v;
	hpl->socket = 0;
	hpl->getting_host_by_name = 0;
	DoHTTPing( hpl->addy, hpl->minperiod, &hpl->seqno, &hpl->timeout_time, &hpl->socket, &hpl->getting_host_by_name, hpl->device );
	hpl->failed = 1;
	return 0;
}


static void * PingRunner( void * v )
{
	struct HTTPPingLaunch *hpl = (struct HTTPPingLaunch*)v;
	hpl->seqno = 0;
	while( 1 )
	{
		hpl->timeout_time = OGGetAbsoluteTime();
		og_thread_t thd = OGCreateThread( DeployPing, hpl );
		while( 1 )
		{
			double Now = OGGetAbsoluteTime();
			double usl = hpl->timeout_time - Now + HTTPTIMEOUT;
			if( usl > 0 ) usleep( (int)(usl*1000000 + 1000));
			else usleep( 10000 );

			if( hpl->timeout_time + HTTPTIMEOUT < Now && !hpl->getting_host_by_name ) //Can't terminate in the middle of a gethostbyname operation otherwise bad things can happen.
			{
				if( hpl->socket )
				{
#ifdef WIN32
					closesocket( hpl->socket );
#else
					close( hpl->socket );
#endif
				}

				OGCancelThread( thd );
				break;
			}
		}
	}
	return 0;
}

int StartHTTPing( const char * addy, double minperiod, const char * device)
{
	struct HTTPPingLaunch *hpl = malloc( sizeof( struct HTTPPingLaunch ) );
	hpl->addy = addy;
	hpl->minperiod = minperiod;
	hpl->device = device;
	OGCreateThread( PingRunner, hpl );
	return 0;
}



