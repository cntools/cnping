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
void ping(struct sockaddr_in *addr , float pingperiod); //If pingperiod = -1, run once and exit.
void do_pinger( const char * strhost, float _ping_period ); //If pingperiod = -1, run once and exit.

void ping_setup();

#endif

