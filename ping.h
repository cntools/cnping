#ifndef _PING_H
#define _PING_H

#include <stdint.h>
#ifdef WIN32
	typedef int socklen_t;
	struct sockaddr;
	#include <winsock2.h>
	#include <ws2tcpip.h>

	// ipv6 icmp ping is not supported on windows anyways
	// this is just needed so it compiles
	#define ICMP6_ECHO_REQUEST 128
	#define ICMP6_ECHO_REPLY 129

	#define ICMP_ECHOREPLY ICMP_ECHO
#else
	#include <sys/socket.h>
	#include <netinet/in.h>
#endif

#ifdef WIN_USE_NO_ADMIN_PING
	#include "rawdraw/os_generic.h"
#endif

// ping data of one host
#define PINGCYCLEWIDTH 8192

struct PreparedPing
{
	// socket for sending the pings
	int fd;

	// the number of this host -> used to access the PingData array
	int pingHostId;

	// address to send the pings to
	struct sockaddr_in6 psaddr;
	// size of psaddr
	socklen_t psaddr_len;

#ifdef WIN_USE_NO_ADMIN_PING
	og_sema_t s_disp;
	og_sema_t s_ping;
#endif
};

// meassured ping data of one host. This is the meassured data cnping displays
struct PingData
{
	double PingSendTimes [PINGCYCLEWIDTH];
	double PingRecvTimes [PINGCYCLEWIDTH];
	int current_cycle;
	double globmaxtime, globmintime;
	double globinterval, globlast;
	uint64_t globalrx;
	uint64_t globallost;

	// pointer to the related prepared ping so it can be freed at exit
	struct PreparedPing* pp;
};

// captured ping data - This is an array that get allocated with the size of pinghostListSize
// each Ping thread should use pingHostId as offset into this array
extern struct PingData * PingData;

unsigned short checksum(const unsigned char *b, uint16_t len);

int createSocket( int family );

//Callback (when received)
void display( uint8_t *buf, int bytes, unsigned int pingHostId );

//Callback (before sending)
//return value = # of bytes to send in ping message.
int load_ping_packet( uint8_t  * buffer, int buffersize, struct PingData * pd );

void listener( struct PreparedPing* pp );
void ping( struct PreparedPing* pp );

void singleping( struct PreparedPing* pp ); // If using this, must call ping_setup( 0, 0); to begin.

//If pingperiodseconds = -1, run ping/do_pinger once and exit.
extern float pingperiodseconds;
extern int ping_failed_to_send;

// resolvs the hostname and prepares the socket. Might return NULL on error
struct PreparedPing* ping_setup(const char * strhost, const char * device);


#endif

