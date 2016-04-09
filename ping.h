#ifndef _PING_H
#define _PING_H

#include <stdint.h>

struct sockaddr_in;

unsigned short checksum(void *b, int len);

//Callback (when received)
void display(uint8_t *buf, int bytes);

//Callback (before sending)
//return value = # of bytes to send in ping message.
int load_ping_packet( uint8_t  * buffer, int buffersize );

void listener(void);
void ping(struct sockaddr_in *addr );
void do_pinger( const char * strhost );

//If pingperiod = -1, run ping/do_pinger once and exit.
extern float pingperiod;
extern int precise_ping; //if 0, use minimal CPU, but ping send-outs are only approximate, if 1, spinlock until precise time for ping is hit.

void ping_setup();

#ifdef WIN32

extern char errbuffer[1024];
#define ERRM(x...) { sprintf( errbuffer, x ); MessageBox( 0, errbuffer, "cnping", 0 ); }

#else

#define ERRM(x...) fprintf( stderr, x );

#endif

#endif

