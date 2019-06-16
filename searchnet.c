#include <stdio.h>
#include "ping.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <stdlib.h>
#include "rawdraw/os_generic.h"

uint32_t my_random_key;
uint8_t send_id[4];

void * PingListen( void * r )
{
	listener();
	printf( "Fault on listen.\n" );
	exit( -2 );
}

void display(uint8_t *buf, int bytes)
{
	uint32_t reqid = ((uint32_t)buf[0+1] << 24) | (buf[1+1]<<16) | (buf[2+1]<<8) | (buf[3+1]);

	if( reqid != my_random_key ) return;

	printf( "%d.%d.%d.%d\n", buf[4+1], buf[5+1], buf[6+1], buf[7+1] );
}

int load_ping_packet( uint8_t * buffer, int bufflen )
{
	buffer[0+1] = my_random_key >> 24;
	buffer[1+1] = my_random_key >> 16;
	buffer[2+1] = my_random_key >> 8;
	buffer[3+1] = my_random_key >> 0;

	buffer[4+1] = send_id[0];
	buffer[5+1] = send_id[1];
	buffer[6+1] = send_id[2];
	buffer[7+1] = send_id[3];

	return 12;
}


int main( int argc, char ** argv )
{
	uint32_t offset;
	int mask;
	in_addr_t base;
	char dispip[32];
	float speed;

	ping_setup();
	OGCreateThread( PingListen, 0 );
	srand( ((int)(OGGetAbsoluteTime()*10000)) );
	my_random_key = rand();

	if( argc != 4 )
	{
		fprintf( stderr, "Usage: [searchnet IP net] [mask (single #, i.e. 24)] [speed (in seconds per attempt)]\n" );
		return -1;
	}

	base = ntohl(inet_addr( argv[1] ));
	mask = 1<<(32-atoi(argv[2]));
	speed = atof(argv[3]);

	base &= ~(mask-1);
	printf( "Base: %08x / Mask: %08x\n", base, mask );
	for( offset = 0; offset < mask; offset++ )
	{
		uint32_t cur = base + offset;
		sprintf( dispip, "%d.%d.%d.%d", (cur>>24)&0xff, (cur>>16)&0xff, (cur>>8)&0xff, (cur)&0xff );
		send_id[0] = (cur>>24)&0xff;
		send_id[1] = (cur>>16)&0xff;
		send_id[2] = (cur>>8)&0xff;
		send_id[3] = (cur)&0xff;
//		printf( "Pinging: %s\n", dispip );
		pingperiodseconds = -1;
		do_pinger( dispip );

		OGUSleep( (int)(speed * 1000000) );
	}

	return 0;
}

