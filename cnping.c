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
float GuiYScaleFactor;
int GuiYscaleFactorIsConstant;

uint8_t pattern[8];


#define PINGCYCLEWIDTH 2048
#define TIMEOUT 4

double PingSendTimes[PINGCYCLEWIDTH];
double PingRecvTimes[PINGCYCLEWIDTH];
int current_cycle = 0;

int ExtraPingSize;

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
	do_pinger( pinghost );
	printf( "Fault on ping.\n" );
	exit( -1 );
}




void HandleKey( int keycode, int bDown ){}
void HandleButton( int x, int y, int button, int bDown ){}
void HandleMotion( int x, int y, int mask ){}

double GetGlobMaxPingTime( void )
{
	int i;
	double maxtime = 0;

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
			if( dt > maxtime ) maxtime = dt;
		}
	}

	return maxtime;
}

void DrawFrame( void )
{
	int i, x, y;

	double now = OGGetAbsoluteTime();
	double globmaxtime = GetGlobMaxPingTime();

	double totaltime = 0;
	int totalcountok = 0;
	double mintime = 10000;
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

		if (!GuiYscaleFactorIsConstant)
		{
			GuiYScaleFactor =  (screeny - 50) / globmaxtime;
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
		}
	}

	stddev /= totalcountok;

	stddev = sqrt(stddev);

	int avg_gui    = avg*GuiYScaleFactor;
	int stddev_gui = stddev*GuiYScaleFactor;

	CNFGColor( 0xff00 );


	int l = avg_gui;
	CNFGTackSegment( 0, screeny-l, screenx, screeny - l );
	l = (avg_gui) + (stddev_gui);
	CNFGTackSegment( 0, screeny-l, screenx, screeny - l );
	l = (avg_gui) - (stddev_gui);
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

const char * glargv[10];
int glargc = 0;

INT_PTR CALLBACK TextEntry( HWND   hwndDlg,
	UINT   uMsg,
	WPARAM wParam,
	LPARAM lParam )
{

//	printf( "%d %d %d\n", uMsg, wParam, lParam );
	switch( uMsg )
	{
	//case IDC_SAVESETS:
	//	GetDlgItemText(DLG_SETS, IDC_SETS1, stringVariable, sizeof(stringVariable));
//	case 31:
//		printf( "Exit\n" );
//		exit( 0 );
//		return 0;
	case WM_INITDIALOG:
		SetDlgItemText(hwndDlg, 4, "0.02");
		SetDlgItemText(hwndDlg, 5, "0" );
		return 0;
	case WM_COMMAND:
		switch( wParam>>24 )
		{
			case 4: case 3: return 0; //keyboard input
			case 1: case 2: return 0; //focus changed.
			case 0:
			{
				int id = wParam & 0xffffff;
				if( id == 8 )
				{
					exit( -1 );
				}

				char Address[1024]; GetDlgItemText(hwndDlg, 3, Address, sizeof(Address));
				char Period[1024]; GetDlgItemText(hwndDlg, 4, Period, sizeof(Period));
				char Extra[1024]; GetDlgItemText(hwndDlg, 5, Extra, sizeof(Extra));
				char Scaling[1024]; GetDlgItemText(hwndDlg, 6, Scaling, sizeof(Scaling));
			
				if( strlen( Address ) )
				{
					glargc = 2;
					glargv[1] = strdup( Address );
					if( strlen( Period ) )
					{
						glargc = 3;
						glargv[2] = strdup( Period );
						if( strlen( Extra ) )
						{
							glargc = 4;
							glargv[3] = strdup( Extra );
							if( strlen( Scaling ) )
							{
								glargc = 5;
								glargv[4] = strdup( Scaling );
							}
						}
					}
				}

//				printf( "+++%s\n", stringVariable );
//				printf( "Commit %p/%d\n", lParam, id );
				EndDialog(hwndDlg, 0);
				return 0; //User pressed enter.
			}
		}

//case IDC_SAVESETS:  http://www.cplusplus.com/forum/beginner/19843/
//GetDlgItemText(DLG_SETS, IDC_SETS1, stringVariable, sizeof(stringVariable));

		//printf( "cmd %p %p %p\n", uMsg, wParam, lParam );
		return 0;
	case WM_CTLCOLORBTN:
		//printf( "ctr %p %p %p\n", uMsg, wParam, lParam );
		//return 0;
	case 32: case 512: case 132: case 24: case 70:
	case 127: case 783: case 28: case 134: case 6: case 7:
	case 8: case 312: case 15: case 71: case 133: case 307:
	case 20: case 310: case 33:
		return 0;
	}
	//MessageBox( 0, "XXX", "cnping", 0 );
	//printf( "XXX %d %d %d\n", uMsg, wParam, lParam );
	return 0;
}
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

#ifdef WIN32
	if( argc < 2 )
	{
		int ret = DialogBox(0, "IPDialog", 0, TextEntry );
		argc = glargc;
		argv = glargv;
	}
#endif

	if( argc < 2 )
	{
#ifdef WIN32
		ERRM( "Need at least a host address to ping.\n" );
#else
		ERRM( "Usage: cnping [host] [period] [extra size] [y-axis scaling]\n"

			  "   [host]                 -- domain or IP address of ping target \n"
			  "   [period]               -- period in seconds (optional), default 0.02 \n"
			  "   [extra size]           -- ping packet extra size (above 12), optional, default = 0 \n"
			  "   [const y-axis scaling] -- use a fixed scaling factor instead of auto scaling (optional)\n");
#endif
		return -1;
	}


	if( argc > 2 )
	{
		pingperiod = atof( argv[2] );
		printf( "Extra ping period: %f\n", pingperiod );
	}
	else
	{
		pingperiod = 0.02;
	}

	if( argc > 3 )
	{
		ExtraPingSize = atoi( argv[3] );
		printf( "Extra ping size:   %d\n", ExtraPingSize );
	}
	else
	{
		ExtraPingSize = 0;
	}

	if( argc > 4 )
	{
		GuiYScaleFactor = atof( argv[4] );
		GuiYscaleFactorIsConstant = 1;
		printf( "GuiYScaleFactor:   %f\n", GuiYScaleFactor );
	}
	else
	{
		printf( "GuiYScaleFactor:   %s\n", "dynamic" );
	}

	pinghost = argv[1];

	sprintf( title, "%s - cnping", pinghost );
	CNFGSetup( title, 320, 155 );

	ping_setup();

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
