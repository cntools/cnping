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
#include <sys/socket.h>
#include <resolv.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <stdlib.h>


#define PACKETSIZE	1500
struct packet
{
	struct icmphdr hdr;
	char msg[PACKETSIZE-sizeof(struct icmphdr)];
};

int pid=-1;
struct protoent *proto=NULL;

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
/*--- display - present echo info                                  ---*/
/*--------------------------------------------------------------------*/
/*void display(void *buf, int bytes)
{	int i;
	struct iphdr *ip = buf;
	struct icmphdr *icmp = buf+ip->ihl*4;

	printf("----------------\n");
	for ( i = 0; i < bytes; i++ )
	{
		if ( !(i & 15) ) printf("\nX:  ", i);
		printf("X ", ((unsigned char*)buf)[i]);
	}
	printf("\n");
	printf("IPv%d: hdr-size=%d pkt-size=%d protocol=%d TTL=%d src=%s ",
		ip->version, ip->ihl*4, ntohs(ip->tot_len), ip->protocol,
		ip->ttl, inet_ntoa(ip->saddr));
	printf("dst=%s\n", inet_ntoa(ip->daddr));
	if ( icmp->un.echo.id == pid )
	{
		printf("ICMP: type[%d/%d] checksum[%d] id[%d] seq[%d]\n",
			icmp->type, icmp->code, ntohs(icmp->checksum),
			icmp->un.echo.id, icmp->un.echo.sequence);
	}
}
*/
/*--------------------------------------------------------------------*/
/*--- listener - separate process to listen for and collect messages--*/
/*--------------------------------------------------------------------*/
void listener(void)
{	int sd;
	struct sockaddr_in addr;
	unsigned char buf[1024];

	sd = socket(PF_INET, SOCK_RAW, proto->p_proto);
	if ( sd < 0 )
	{
		perror("socket");
		exit(0);
	}
	for (;;)
	{	int bytes, len=sizeof(addr);

		bzero(buf, sizeof(buf));
		bytes = recvfrom(sd, buf, sizeof(buf), 0, (struct sockaddr*)&addr, &len);
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
void ping(struct sockaddr_in *addr, float pingperiod)
{	const int val=255;
	int i, sd, cnt=1;
	struct packet pckt;
	struct sockaddr_in r_addr;

	sd = socket(PF_INET, SOCK_RAW, proto->p_proto);
	if ( sd < 0 )
	{
		perror("socket");
		return;
	}
	if ( setsockopt(sd, SOL_IP, IP_TTL, &val, sizeof(val)) != 0)
		perror("Set TTL option");
	if ( fcntl(sd, F_SETFL, O_NONBLOCK) != 0 )
		perror("Request nonblocking I/O");

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
		pckt.msg[i] = 0;
		pckt.hdr.un.echo.sequence = cnt++;
		pckt.hdr.checksum = checksum(&pckt, sizeof(pckt) - sizeof( pckt.msg ) + rsize );
		if ( sendto(sd, &pckt, sizeof(pckt) - sizeof( pckt.msg ) + rsize , 0, (struct sockaddr*)addr, sizeof(*addr)) <= 0 )
			perror("sendto");

		if( pingperiod > 0 )
		{
			uint32_t dlw = 1000000.0*pingperiod;
			usleep( dlw );
		}
	} 	while( pingperiod >= 0 );
	close( sd );
}

void ping_setup()
{
	pid = getpid();
	proto = getprotobyname("ICMP");
}

void do_pinger( const char * strhost, float _ping_period )
{
	struct sockaddr_in addr;
	struct hostent *hname;
	hname = gethostbyname(strhost);

	bzero(&addr, sizeof(addr));
	addr.sin_family = hname->h_addrtype;
	addr.sin_port = 0;
	addr.sin_addr.s_addr = *(long*)hname->h_addr;
	ping(&addr, _ping_period);
}

/*--------------------------------------------------------------------*/
/*--- main - look up host and start ping processes.                ---*/
/*--------------------------------------------------------------------*/
int pingmain(int count, char *strings[])
{	struct hostent *hname;
	struct sockaddr_in addr;

	if ( count != 2 )
	{
		printf("usage: %s <addr>\n", strings[0]);
		exit(0);
	}
	if ( count > 1 )
	{
		pid = getpid();
		proto = getprotobyname("ICMP");
		hname = gethostbyname(strings[1]);
		bzero(&addr, sizeof(addr));
		addr.sin_family = hname->h_addrtype;
		addr.sin_port = 0;
		addr.sin_addr.s_addr = *(long*)hname->h_addr;
		if ( fork() == 0 )
			listener();
		else
			ping(&addr, 1);
		wait(0);
	}
	else
		printf("usage: myping <hostname>\n");
	return 0;
}

