#ifndef _PING_H
#define _PING_H

#include <stdint.h>

struct sockaddr_in;

unsigned short checksum(const unsigned char *b, uint16_t len);

//Callback (when received)
void display(uint8_t *buf, int bytes);

//Callback (before sending)
//return value = # of bytes to send in ping message.
int load_ping_packet( uint8_t  * buffer, int buffersize );

void listener();
void ping(struct sockaddr_in *addr );
void do_pinger( const char * strhost );

//If pingperiodseconds = -1, run ping/do_pinger once and exit.
extern float pingperiodseconds;
extern int precise_ping; //if 0, use minimal CPU, but ping send-outs are only approximate, if 1, spinlock until precise time for ping is hit.
extern int ping_failed_to_send;
void ping_setup();

#ifdef WIN32

extern char errbuffer[1024];
#ifndef _MSC_VER
#define ERRM(x...) { sprintf( errbuffer, x ); MessageBox( 0, errbuffer, "cnping", 0 ); }
#else
#define ERRM(...) { sprintf( errbuffer, __VA_ARGS__ ); MessageBox( 0, errbuffer, "cnping", 0 ); }
#endif

#else

#define ERRM(x...) fprintf( stderr, x );

#endif

#endif

