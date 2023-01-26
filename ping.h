#ifndef _PING_H
#define _PING_H

#include <stdint.h>
#ifdef WIN32
typedef int socklen_t;
struct sockaddr;
#else
#include <sys/socket.h>
#endif

unsigned short checksum( const unsigned char *b, uint16_t len );

// Callback (when received)
void display( uint8_t *buf, int bytes );

// Callback (before sending)
// return value = # of bytes to send in ping message.
int load_ping_packet( uint8_t *buffer, int buffersize );

void listener();
void ping( struct sockaddr *addr, socklen_t addr_len );
void do_pinger();

// If pingperiodseconds = -1, run ping/do_pinger once and exit.
extern float pingperiodseconds;
extern int precise_ping; // if 0, use minimal CPU, but ping send-outs are only approximate, if 1,
                         // spinlock until precise time for ping is hit.
extern int ping_failed_to_send;
void ping_setup( const char *strhost, const char *device );


#endif
