//Copyright (c) 2011-2014 <>< Charles Lohr - Under the MIT/x11 or NewBSD License you choose.

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#ifdef WIN32
#include <windows.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <netdb.h>
#endif
#include "DrawFunctions.h"
#include "os_generic.h"
#include "ping.h"

unsigned frames = 0;
unsigned long iframeno = 0;
short screenx, screeny;
const char * pinghost;
float pingperiod;
double GuiYScaleFactor;

uint8_t pattern[8];


#define PINGCYCLEWIDTH 1024
#define TIMEOUT 4

double PingSendTimes[PINGCYCLEWIDTH];
double PingRecvTimes[PINGCYCLEWIDTH];
int current_cycle = 0;

int ExtraPingSize = 0;

void display(uint8_t *buf, int bytes)
{
	int reqid = (buf[0] << 24) | (buf[1]<<16) | (buf[2]<<8) | (buf[3]);
	reqid &= (PINGCYCLEWIDTH-1);

	double STime = PingSendTimes[reqid];
	double LRTime = PingRecvTimes[reqid];

	if( memcmp( buf+4, pattern, 8 ) != 0 ) return;
	if( LRTime > STime ) return;
	if( STime < 1 )  return;

	//Otherwise this is a legit packet.

	PingRecvTimes[reqid] = OGGetAbsoluteTime();
}

int load_ping_packet( uint8_t * buffer, int bufflen )
{
	buffer[0] = current_cycle >> 24;
	buffer[1] = current_cycle >> 16;
	buffer[2] = current_cycle >> 8;
	buffer[3] = current_cycle >> 0;

	memcpy( buffer+4, pattern, 8 );

	PingSendTimes[current_cycle&(PINGCYCLEWIDTH-1)] = OGGetAbsoluteTime();
	PingRecvTimes[current_cycle&(PINGCYCLEWIDTH-1)] = 0;

	current_cycle++;

	return 12 + ExtraPingSize;
}

void * PingListen( void * r )
{
	listener();
	printf( "Fault on listen.\n" );
	exit( -2 );
}

void * PingSend( void * r )
{
	do_pinger( pinghost, pingperiod );
	printf( "Fault on ping.\n" );
	exit( -1 );
}




void HandleKey( int keycode, int bDown ){}
void HandleButton( int x, int y, int button, int bDown ){}
void HandleMotion( int x, int y, int mask ){}
void DrawFrame( void )
{
	int i, x, y;

	double now = OGGetAbsoluteTime();

	double totaltime = 0;
	int totalcountok = 0;
	double mintime = 100;
	double maxtime = 0;
	double stddev = 0;
	double last = -1;

	for( i = 0; i < screenx; i++ )
	{
		int index = ((current_cycle - i - 1) + PINGCYCLEWIDTH) & (PINGCYCLEWIDTH-1);
		double st = PingSendTimes[index];
		double rt = PingRecvTimes[index];

		double dt = 0;

		if( rt > st )
		{
			CNFGColor( 0xffffff );
			dt = rt - st;
			dt *= 1000;
			totaltime += dt;
			if( dt < mintime ) mintime = dt;
			if( dt > maxtime ) maxtime = dt;
			totalcountok++;
		}
		else
		{
			CNFGColor( 0xff );
			dt = now - st;
			dt *= 1000;

		}

		if( last < 0 && rt > st )
			last = dt;
		int h = dt*GuiYScaleFactor;
		int top = screeny - h;
		if( top < 0 ) top = 0;
		CNFGTackSegment( i, screeny-1, i, top );
	}

	double avg = totaltime / totalcountok;

	for( i = 0; i < screenx; i++ )
	{
		int index = ((current_cycle - i - 1) + PINGCYCLEWIDTH) & (PINGCYCLEWIDTH-1);
		double st = PingSendTimes[index];
		double rt = PingRecvTimes[index];

		double dt = 0;
		if( rt > st )
		{
			dt = rt - st;
			dt *= 1000;
			stddev += (dt-avg)*(dt-avg);
			//dt *= GraphScaleFactor;

		}
	}

	stddev /= totalcountok;

	stddev = sqrt(stddev);

	CNFGColor( 0xff00 );
	int l = avg;
	CNFGTackSegment( 0, screeny-l, screenx, screeny - l );
	l = avg + stddev;
	CNFGTackSegment( 0, screeny-l, screenx, screeny - l );
	l = avg - stddev;
	CNFGTackSegment( 0, screeny-l, screenx, screeny - l );

	char stbuf[1024];
	char * sptr = &stbuf[0];
	sptr += sprintf( sptr, "Last: %5.2f ms\n", last );
	sptr += sprintf( sptr, "Min : %5.2f ms\n", mintime );
	sptr += sprintf( sptr, "Max : %5.2f ms\n", maxtime );
	sptr += sprintf( sptr, "Avg : %5.2f ms\n", avg );
	sptr += sprintf( sptr, "Std : %5.2f ms\n", stddev );
	CNFGColor( 0x00 );
	for( x = -1; x < 2; x++ ) for( y = -1; y < 2; y++ )
	{
		CNFGPenX = 10+x; CNFGPenY = 10+y;
		CNFGDrawText( stbuf, 2 );
	}
	CNFGColor( 0xffffff );
	CNFGPenX = 10; CNFGPenY = 10;
	CNFGDrawText( stbuf, 2 );
	OGUSleep( 1000 );
}

#ifdef WIN32
int mymain( int argc, const char ** argv )
#else
int main( int argc, const char ** argv )
#endif
{
	char title[1024];
	int i, x, y, r;
	double ThisTime;
	double LastFPSTime = OGGetAbsoluteTime();
	double LastFrameTime = OGGetAbsoluteTime();
	double SecToWait;
	int linesegs = 0;
//	struct in_addr dst;
	struct addrinfo *result;

	srand( (int)(OGGetAbsoluteTime()*100000) );

	for( i = 0; i < sizeof( pattern ); i++ )
	{
		pattern[i] = rand();
	}
	CNFGBGColor = 0x800000;
	CNFGDialogColor = 0x444444;
	for( i = 0; i < PINGCYCLEWIDTH; i++ )
	{
		PingSendTimes[i] = 0;
		PingRecvTimes[i] = 0;
	}

	if( argc < 2 )
	{
		ERRM( "Usage: cnping [host] [period] [extra size] [y-axis scaling]\n"

			  "   [host]           -- domain or IP address of ping target \n"
			  "   [period]         -- period in seconds (optional), default 0.02 \n"
			  "   [extra size]     -- ping packet extra size (above 12), optional, default = 0 \n"
			  "   [y-axis scaling] -- see more swing for small values (optional), default = 1\n");
		return -1;
	}

	if( argc > 3 )
	{
		ExtraPingSize = atoi( argv[3] );
		printf( "Extra ping size added: %d\n", ExtraPingSize );
	}

	sprintf( title, "%s - cnping", argv[1] );
	CNFGSetup( title, 320, 155 );

	ping_setup();

	pinghost = argv[1];
	pingperiod = (argc>=3)?atof( argv[2] ):.02;

	GuiYScaleFactor = (argc>=4)?atof( argv[4] ):1;

	OGCreateThread( PingSend, 0 );
	OGCreateThread( PingListen, 0 );

	while(1)
	{
		int i, pos;
		float f;
		iframeno++;
		RDPoint pto[3];

		CNFGHandleInput();

		CNFGClearFrame();
		CNFGColor( 0xFFFFFF );
		CNFGGetDimensions( &screenx, &screeny );

		DrawFrame();

		frames++;
		CNFGSwapBuffers();

		ThisTime = OGGetAbsoluteTime();
		if( ThisTime > LastFPSTime + 1 )
		{
			frames = 0;
			linesegs = 0;
			LastFPSTime+=1;
		}

		SecToWait = .030 - ( ThisTime - LastFrameTime );
		LastFrameTime += .030;
		if( SecToWait > 0 )
			OGUSleep( (int)( SecToWait * 1000000 ) );
	}

	return(0);
}

#ifdef WIN32

//from: http://alter.org.ua/docs/win/args/
 PCHAR*
    CommandLineToArgvA(
        PCHAR CmdLine,
        int* _argc
        )
    {
        PCHAR* argv;
        PCHAR  _argv;
        ULONG   len;
        ULONG   argc;
        CHAR   a;
        ULONG   i, j;

        BOOLEAN  in_QM;
        BOOLEAN  in_TEXT;
        BOOLEAN  in_SPACE;

        len = strlen(CmdLine);
        i = ((len+2)/2)*sizeof(PVOID) + sizeof(PVOID);

        argv = (PCHAR*)GlobalAlloc(GMEM_FIXED,
            i + (len+2)*sizeof(CHAR));

        _argv = (PCHAR)(((PUCHAR)argv)+i);

        argc = 0;
        argv[argc] = _argv;
        in_QM = FALSE;
        in_TEXT = FALSE;
        in_SPACE = TRUE;
        i = 0;
        j = 0;

        while( a = CmdLine[i] ) {
            if(in_QM) {
                if(a == '\"') {
                    in_QM = FALSE;
                } else {
                    _argv[j] = a;
                    j++;
                }
            } else {
                switch(a) {
                case '\"':
                    in_QM = TRUE;
                    in_TEXT = TRUE;
                    if(in_SPACE) {
                        argv[argc] = _argv+j;
                        argc++;
                    }
                    in_SPACE = FALSE;
                    break;
                case ' ':
                case '\t':
                case '\n':
                case '\r':
                    if(in_TEXT) {
                        _argv[j] = '\0';
                        j++;
                    }
                    in_TEXT = FALSE;
                    in_SPACE = TRUE;
                    break;
                default:
                    in_TEXT = TRUE;
                    if(in_SPACE) {
                        argv[argc] = _argv+j;
                        argc++;
                    }
                    _argv[j] = a;
                    j++;
                    in_SPACE = FALSE;
                    break;
                }
            }
            i++;
        }
        _argv[j] = '\0';
        argv[argc] = NULL;

        (*_argc) = argc;
        return argv;
    }


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	int argc;
	char cts[8192];
	sprintf( cts, "%s %s", GetCommandLine(), lpCmdLine );

	ShowWindow (GetConsoleWindow(), SW_HIDE);
	char ** argv = CommandLineToArgvA(
        cts,
        &argc
        );
//	MessageBox( 0, argv[1], "X", 0 );
	return mymain( argc-1, (const char**)argv );
}

#endif
