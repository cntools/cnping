/* myping.c
 *
 * Copyright (c) 2000 Sean Walton and Macmillan Publishers.  Use may be in
 * whole or in part in accordance to the General Public License (GPL).
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
*/

/*****************************************************************************/
/*** myping.c                                                              ***/
/***                                                                       ***/
/*** Use the ICMP protocol to request "echo" from destination.             ***/
/*****************************************************************************/


#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <strings.h>
#include "ping.h"
#include "os_generic.h"
#ifdef WIN32
#include <winsock2.h>
#define SOL_IP       0
#define F_SETFL              4
#define ICMP_ECHO             8
#define IP_TTL 2
# define O_NONBLOCK   04000
void bzero(void *location,__LONG32 count);
#include <windows.h>
#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdint.h>
struct icmphdr {
  uint8_t		type;
  uint8_t		code;
  uint16_t	checksum;
  union {
	struct {
		uint16_t	id;
		uint16_t	sequence;
	} echo;
	uint32_t	gateway;
	struct {
		uint16_t	__unused;
		uint16_t	mtu;
	} frag;
  } un;
};
#else
#include <sys/socket.h>
#include <resolv.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#endif
#include <stdlib.h>

float pingperiod;
int precise_ping;

#define PACKETSIZE	1500
struct packet
{
	struct icmphdr hdr;
	char msg[PACKETSIZE-sizeof(struct icmphdr)];
};

int sd;
int pid=-1;
struct protoent *proto=NULL;
struct sockaddr_in psaddr;

/*--------------------------------------------------------------------*/
/*--- checksum - standard 1s complement checksum                   ---*/
/*--------------------------------------------------------------------*/
unsigned short checksum(void *b, int len)
{	unsigned short *buf = b;
	unsigned int sum=0;
	unsigned short result;

	for ( sum = 0; len > 1; len -= 2 )
		sum += *buf++;
	if ( len == 1 )
		sum += *(unsigned char*)buf;
	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	result = ~sum;
	return result;
}


/*--------------------------------------------------------------------*/
/*--- listener - separate process to listen for and collect messages--*/
/*--------------------------------------------------------------------*/
void listener(void)
{
#ifndef WIN32
	const int val=255;

	int sd = socket(PF_INET, SOCK_RAW, proto->p_proto);

	if ( setsockopt(sd, SOL_IP, IP_TTL, &val, sizeof(val)) != 0)
	{
		ERRM("Erro: could not set TTL option\n");
			exit( -1 );
	}

#endif

	struct sockaddr_in addr;
	unsigned char buf[1024];

	for (;;)
	{
		int i;
		int bytes, len=sizeof(addr);

		bzero(buf, sizeof(buf));
		bytes = recvfrom(sd, buf, sizeof(buf), 0, (struct sockaddr*)&addr, &len);
		if( buf[20] != 0 ) continue; //Make sure ping response.
		if( buf[9] != 1 ) continue; //ICMP
		if( addr.sin_addr.s_addr != psaddr.sin_addr.s_addr ) continue;

		if ( bytes > 0 )
			display(buf + 28, bytes - 28 );
		else
			perror("recvfrom");
	}
	printf( "Fault on listen.\n" );
	exit(0);
}

/*--------------------------------------------------------------------*/
/*--- ping - Create message and send it.                           ---*/
/*--------------------------------------------------------------------*/
void ping(struct sockaddr_in *addr )
{

#ifdef WIN32
	const char val=255;
#else
	const int val=255;
#endif
	int i, cnt=1;
	struct packet pckt;
	struct sockaddr_in r_addr;
#ifdef WIN32
	{
		//this /was/ recommended.
		unsigned long iMode = 1;
		ioctlsocket(sd, FIONBIO, &iMode);
	}
#else
	if ( fcntl(sd, F_SETFL, O_NONBLOCK) != 0 )
		ERRM("Warning: Request nonblocking I/O failed.");
#endif

	double stime = OGGetAbsoluteTime();

	do
	{	int len=sizeof(r_addr);

//		printf("Msg #%d\n", cnt);
//		if ( recvfrom(sd, &pckt, sizeof(pckt), 0, (struct sockaddr*)&r_addr, &len) > 0 )
		{
			//printf("***Got message!***\n");
		}
		bzero(&pckt, sizeof(pckt));
		pckt.hdr.type = ICMP_ECHO;
		pckt.hdr.un.echo.id = pid;
		int rsize = load_ping_packet( pckt.msg, sizeof( pckt.msg ) );
		pckt.hdr.un.echo.sequence = cnt++;
		pckt.hdr.checksum = checksum(&pckt, sizeof(pckt) - sizeof( pckt.msg ) + rsize );
		if ( sendto(sd, (char*)&pckt, sizeof(pckt) - sizeof( pckt.msg ) + rsize , 0, (struct sockaddr*)addr, sizeof(*addr)) <= 0 )
			perror("sendto");


		if( precise_ping )
		{
			double ctime;
			do
			{
				ctime = OGGetAbsoluteTime();
				if( pingperiod >= 1000 ) stime = ctime;
			} while( ctime < stime + pingperiod );
			stime += pingperiod;
		}
		else
		{
			if( pingperiod > 0 )
			{
				uint32_t dlw = 1000000.0*pingperiod;
				usleep( dlw );
			}
		}
	} 	while( pingperiod >= 0 );
	//close( sd ); //Hacky, we don't close here because SD doesn't come from here, rather  from ping_setup.  We may want to run this multiple times.
}

void ping_setup()
{
	pid = getpid();
	proto = getprotobyname("ICMP");

#ifdef WIN32
/*
    WSADATA wsaData;
	int r = WSAStartup(0x0202, &wsaData );
*/
	WSADATA wsaData;
	int r =	WSAStartup(MAKEWORD(2,2), &wsaData);
	if( r )
	{
		ERRM( "Fault in WSAStartup\n" );
		exit( -2 );
	}
#endif


#ifdef WIN32
	sd = WSASocket(AF_INET, SOCK_RAW, IPPROTO_ICMP, 0, 0, 0);
	{
		int lttl = 0xff;
		if (setsockopt(sd, IPPROTO_IP, IP_TTL, (const char*)&lttl, 
				sizeof(lttl)) == SOCKET_ERROR) {
			printf( "Warning: No IP_TTL.\n" );
		}
	}
#else
	const int val=255;

	sd = socket(PF_INET, SOCK_RAW, proto->p_proto);

	if ( setsockopt(sd, SOL_IP, IP_TTL, &val, sizeof(val)) != 0)
	{
		ERRM("Error: Failed to set TTL option\n");
		exit( -1 );
	}

#endif
	if ( sd < 0 )
	{
		ERRM("Error: Could not create raw socket\n");
		exit(0);
	}

}

void do_pinger( const char * strhost )
{
	struct hostent *hname;
	hname = gethostbyname(strhost);

	bzero(&psaddr, sizeof(psaddr));
	psaddr.sin_family = hname->h_addrtype;
	psaddr.sin_port = 0;
	psaddr.sin_addr.s_addr = *(long*)hname->h_addr;
	ping(&psaddr );
}

char errbuffer[1024];


