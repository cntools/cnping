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

// ping data of one host
#define PINGCYCLEWIDTH 8192

struct PreparedPing
{
	int fd;
	int pingHostId;
	struct sockaddr_in6 psaddr;
	socklen_t psaddr_len;
};

struct PingData
{
	double PingSendTimes [PINGCYCLEWIDTH];
	double PingRecvTimes [PINGCYCLEWIDTH];
	int current_cycle;
	double globmaxtime, globmintime;
	double globinterval, globlast;
	uint64_t globalrx;
	uint64_t globallost;
	struct PreparedPing* pp;
};

extern struct PingData * PingData;

unsigned short checksum(const unsigned char *b, uint16_t len);

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
struct PreparedPing* ping_setup(const char * strhost, const char * device);


#endif

