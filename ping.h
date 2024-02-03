#ifndef _PING_H
#define _PING_H

#include <stdint.h>
#ifdef WIN32
	typedef int socklen_t;
	struct sockaddr;
#else
	#include <sys/socket.h>
#endif

// ping data of one host
#define PINGCYCLEWIDTH 8192
struct PingData
{
	double PingSendTimes [PINGCYCLEWIDTH];
	double PingRecvTimes [PINGCYCLEWIDTH];
	int current_cycle;
	double globmaxtime, globmintime;
	double globinterval, globlast;
	uint64_t globalrx;
	uint64_t globallost;
};

extern struct PingData * PingData;

unsigned short checksum(const unsigned char *b, uint16_t len);

//Callback (when received)
void display( uint8_t *buf, int bytes, unsigned int pingHostId );

//Callback (before sending)
//return value = # of bytes to send in ping message.
int load_ping_packet( uint8_t  * buffer, int buffersize, struct PingData * pd );

void listener( unsigned int pingHostId );
void ping( unsigned int pingHostId, struct sockaddr *addr, socklen_t addr_len );
void do_pinger( );

void singleping( unsigned int pingHostId, struct sockaddr *addr, socklen_t addr_len ); // If using this, must call ping_setup( 0, 0); to begin.

//If pingperiodseconds = -1, run ping/do_pinger once and exit.
extern float pingperiodseconds;
extern int precise_ping; //if 0, use minimal CPU, but ping send-outs are only approximate, if 1, spinlock until precise time for ping is hit.
extern int ping_failed_to_send;
void ping_setup(const char * strhost, const char * device);


#endif

