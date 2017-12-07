//Copyright (c) 2011-2014 <>< Charles Lohr - Under the MIT/x11 or NewBSD License you choose.

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <argp.h>
#ifdef WIN32
#ifdef _MSC_VER
#define strdup _strdup
#endif
#include <windows.h>
#else
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <netdb.h>
#endif
#include "CNFGFunctions.h"
#include "os_generic.h"
#include "ping.h"


const char *argp_program_version = "cnping";
const char *argp_program_bug_address = "https://github.com/cnlohr/cnping";
const char doc[] = "\ncnping -- Minimal Graphical IPV4 Ping Tool.";

static char args_doc[] = "HOST";

static struct argp_option options[] = {
	/* {"host",	'h', 0, 0, "domain or IP address of ping target"}, */
	/* {"target",	't', 0, OPTION_ALIAS}, */
	{"period",	'p', 0, OPTION_ARG_OPTIONAL, "period in seconds (optional), default 0.02"},
	{"extra-size",	'x', 0, OPTION_ARG_OPTIONAL, "ping packet extra size (above 12), optional, default = 0"},
	{"scale",	's', 0, OPTION_ARG_OPTIONAL, "use a fixed scaling factor instead of auto scaling"},
	{"title", 't', 0, OPTION_ARG_OPTIONAL, "the window title"},
	{0}
};

struct arguments
{
	char *host;
	float period;
	int extrasize;
	float scale;
	char *title;
};

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
	struct arguments *arguments	= state->input;

	switch(key)
	{
		case ARGP_KEY_ARG:
			if (state->arg_num > 1)
				argp_usage(state);
			arguments->host = arg;
			break;
		case ARGP_KEY_END:
			if (state->arg_num<1)
				argp_usage(state);
			break;
		case 'p':
			arguments->period = atof(arg);
			break;
		case 'x':
			arguments->extrasize = atoi(arg);
			break;
		case 's':
			arguments->scale = atof(arg);
			break;
		case 't':
			arguments->title = arg;
			break;
		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp argp = {options, parse_opt, args_doc, doc};

unsigned frames = 0;
unsigned long iframeno = 0;
short screenx, screeny;
const char * pinghost;
float GuiYScaleFactor;
int GuiYscaleFactorIsConstant;

uint8_t pattern[8];


#define PINGCYCLEWIDTH 8192
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

	if( ping_failed_to_send )
	{
		PingSendTimes[(current_cycle+PINGCYCLEWIDTH-1)&(PINGCYCLEWIDTH-1)] = 0; //Unset ping send.
	}

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




void HandleKey( int keycode, int bDown )
{
	switch( keycode )
	{
		case 'q':
			exit(0);
			break;

	}
}
void HandleButton( int x, int y, int button, int bDown ){}
void HandleMotion( int x, int y, int mask ){}
void HandleDestroy() { exit(0); }

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
	int totalcountloss = 0;
	double mintime = 10000;
	double maxtime = 0;
	double stddev = 0;
	double last = -1;
	double loss = 100.00;

	for( i = 0; i < screenx; i++ )
	{
		int index = ((current_cycle - i - 1) + PINGCYCLEWIDTH) & (PINGCYCLEWIDTH-1);
		double st = PingSendTimes[index];
		double rt = PingRecvTimes[index];

		double dt = 0;

		if( rt > st ) // ping received
		{
			CNFGColor( 0xffffff );
			dt = rt - st;
			dt *= 1000;
			totaltime += dt;
			if( dt < mintime ) mintime = dt;
			if( dt > maxtime ) maxtime = dt;
			totalcountok++;
			if( last < 0)
				last = dt;
		}
		else if (st != 0) // ping sent but not received
		{
			CNFGColor( 0xff );
			dt = now - st;
			dt *= 1000;
			if( i != 0 ) totalcountloss++;
		}
		else // no ping sent for this point in time (after startup)
		{
			CNFGColor( 0x0 );
			dt = 99 * 1000; // assume 99s to fill screen black
		}

		if (!GuiYscaleFactorIsConstant)
		{
			GuiYScaleFactor =  (screeny - 50) / globmaxtime;
		}

		int h = dt*GuiYScaleFactor;
		int top = screeny - h;
		if( top < 0 ) top = 0;
		CNFGTackSegment( i, screeny-1, i, top );
	}

	double avg = totaltime / totalcountok;
	loss = (double) totalcountloss / (totalcountok + totalcountloss) * 100;

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

	sptr += sprintf( sptr, 
		"Last: %5.2f ms\n"
		"Min : %5.2f ms\n"
		"Max : %5.2f ms\n"
		"Avg : %5.2f ms\n"
		"Std : %5.2f ms\n"
		"Loss: %5.1f %%\n\n%s", last, mintime, maxtime, avg, stddev, loss, (ping_failed_to_send?"Could not send ping.\nIs target reachable?\nDo you have sock_raw to privileges?":"") );

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

INT_PTR CALLBACK TextEntry( HWND   hwndDlg, UINT   uMsg, WPARAM wParam, LPARAM lParam )
{

	switch( uMsg )
	{
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
				EndDialog(hwndDlg, 0);
				return 0; //User pressed enter.
			}
		}
/*		return 0;
	case WM_CTLCOLORBTN:
		//printf( "ctr %p %p %p\n", uMsg, wParam, lParam );
		//return 0;
	case 32: case 512: case 132: case 24: case 70:
	case 127: case 783: case 28: case 134: case 6: case 7:
	case 8: case 312: case 15: case 71: case 133: case 307:
	case 20: case 310: case 33:
		return 0;
*/
	}
	return 0;
}
#endif
int main( int argc, char ** argv )
{
	char title[1024] = "cnping";
	int i;
	double ThisTime;
	double LastFPSTime = OGGetAbsoluteTime();
	double LastFrameTime = OGGetAbsoluteTime();
	double SecToWait;

	struct arguments arguments;

	arguments.host = NULL;
	arguments.period = 0.02;
	arguments.extrasize = 12;
	arguments.scale = 1;
	arguments.title = NULL;

	argp_parse(&argp, argc, argv, 0, 0, &arguments);

	printf("arguments.host = %s\n", arguments.host);
	printf("arguments.period = %f\n", arguments.period);
	printf("arguments.extrasize = %d\n", arguments.extrasize);
	printf("arguments.scale = %f\n", arguments.scale);
	printf("arguments.title = %s\n", arguments.title);

#ifdef WIN32
	ShowWindow (GetConsoleWindow(), SW_HIDE);
#endif

	srand( (int)(OGGetAbsoluteTime()*100000) );

	for( i = 0; i < sizeof( pattern ); i++ )
	{
		pattern[i] = rand();
	}
	CNFGBGColor = 0x800000;
	CNFGDialogColor = 0x444444;
#ifdef WIN32
	if( argc < 2 )
	{
		DialogBox(0, "IPDialog", 0, TextEntry );
		argc = glargc;
		argv = glargv;
	}

	pingperiodseconds = (argc > 2)?atof( argv[2] ):0.02;
	ExtraPingSize = (argc > 3)?atoi( argv[3] ):0;

	if( argc > 4 )
	{
		GuiYScaleFactor = atof( argv[4] );
		GuiYscaleFactorIsConstant = 1;
	}

	pinghost = argv[1];

	sprintf( title, "%s - cnping", pinghost );
#endif

	/* pingperiodseconds = (argc > 2)?atof( argv[2] ):0.02; */
	//TODO
	pingperiodseconds = arguments.period;
	/* ExtraPingSize = (argc > 3)?atoi( argv[3] ):0; */
	//TODO
	ExtraPingSize = arguments.extrasize;

	/* if( argc > 4 ) */
	/* { */
	/* 	GuiYScaleFactor = atof( argv[4] ); */
	/* 	GuiYscaleFactorIsConstant = 1; */
	/* } */
	//TODO
	GuiYScaleFactor = arguments.scale;
	/* GuiYscaleFactorIsConstant = 1; */

	/* pinghost = argv[1]; */
	//TODO
	pinghost = arguments.host;

	/* sprintf( title, "%s - cnping", pinghost ); */
	//TODO
	if (arguments.host)
		sprintf( title, "%s - cnping", pinghost);
	else
		sprintf( title, "%s", arguments.host);

	CNFGSetup( title, 320, 155 );

	ping_setup();

	OGCreateThread( PingSend, 0 );
	OGCreateThread( PingListen, 0 );

	while(1)
	{
		iframeno++;
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
			LastFPSTime+=1;
		}

		SecToWait = .030 - ( ThisTime - LastFrameTime );
		LastFrameTime += .030;
		if( SecToWait > 0 )
			OGUSleep( (int)( SecToWait * 1000000 ) );
	}

	return(0);
}

