//Copyright (c) 2011-2014 <>< Charles Lohr - Under the MIT/x11 or NewBSD License you choose.

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>
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
#include "error_handling.h"
#include "httping.h"

unsigned frames = 0;
unsigned long iframeno = 0;
short screenx, screeny;
const char * pinghost;
float GuiYScaleFactor;
int GuiYscaleFactorIsConstant;
double globmaxtime, globmintime = 1e20;
double globinterval, globlast;
uint64_t globalrx;
uint64_t globallost;
uint8_t pattern[8];


#define PINGCYCLEWIDTH 8192
#define TIMEOUT 4

double PingSendTimes[PINGCYCLEWIDTH];
double PingRecvTimes[PINGCYCLEWIDTH];
int current_cycle = 0;

int ExtraPingSize;
int in_histogram_mode, in_frame_mode = 1;
void HandleGotPacket( int seqno, int timeout );

#define MAX_HISTO_MARKS (TIMEOUT*10000)
uint64_t hist_counts[MAX_HISTO_MARKS];

void HandleNewPacket( int seqno )
{
	double Now = OGGetAbsoluteTime();
	PingSendTimes[seqno] = Now;
	PingRecvTimes[seqno] = 0;
	static int timeoutmark;

	while( Now - PingSendTimes[timeoutmark] > TIMEOUT )
	{
		if( PingRecvTimes[timeoutmark] < PingSendTimes[timeoutmark] )
		{
			HandleGotPacket( timeoutmark, 1 );
		}
		timeoutmark++;
		if( timeoutmark >= PINGCYCLEWIDTH ) timeoutmark = 0;
	}
}

void HandleGotPacket( int seqno, int timeout )
{
	double Now = OGGetAbsoluteTime();

	if( timeout )
	{
		if( PingRecvTimes[seqno] < -0.5 ) return;

		globallost++;
		PingRecvTimes[seqno] = -1;
		hist_counts[MAX_HISTO_MARKS-1]++;
		return;
	}

	if( PingRecvTimes[seqno] >= PingSendTimes[seqno] ) return;
	if( PingSendTimes[seqno] < 1 )  return;
	if( Now - PingSendTimes[seqno] > TIMEOUT ) return;

	PingRecvTimes[seqno] = OGGetAbsoluteTime();
	double Delta = PingRecvTimes[seqno] - PingSendTimes[seqno];
	if( Delta > globmaxtime ) { globmaxtime = Delta; }
	if( Delta < globmintime ) { globmintime = Delta; }
	int slot = Delta * 10000;
	if( slot >= MAX_HISTO_MARKS ) slot = MAX_HISTO_MARKS-1;
	if( slot < 0 ) slot = 0;
	hist_counts[slot]++;

	if( globlast > 0.5 )
	{
		if( Now - globlast > globinterval ) globinterval = Now - globlast;
	}
	globlast = Now;
	globalrx++;
}


void HTTPingCallbackStart( int seqno )
{
	current_cycle = seqno;
	HandleNewPacket( seqno );
}

void HTTPingCallbackGot( int seqno )
{
	HandleGotPacket( seqno, 0 );
}

void display(uint8_t *buf, int bytes)
{
	int reqid = (buf[0] << 24) | (buf[1]<<16) | (buf[2]<<8) | (buf[3]);
	reqid &= (PINGCYCLEWIDTH-1);
	if( memcmp( buf+4, pattern, 8 ) != 0 ) return;
	HandleGotPacket( reqid, 0 );
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

	HandleNewPacket( current_cycle&(PINGCYCLEWIDTH-1) );

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
		case 'f':
			if( bDown ) in_frame_mode = !in_frame_mode;
			if( !in_frame_mode ) in_histogram_mode = 1;
			break;
		case 'm': 
			if( bDown ) in_histogram_mode = !in_histogram_mode;
			if( !in_histogram_mode ) in_frame_mode = 1;
			break;
		case 'c':
			memset( hist_counts, 0, sizeof( hist_counts ) );
			globmaxtime = 0;
			globmintime = 1e20;
			globinterval = 0;
			globlast = 0;
			globalrx = 0;
			globallost = 0;
			break;
		case 'q':
			exit(0);
			break;

	}
}
void HandleButton( int x, int y, int button, int bDown ){}
void HandleMotion( int x, int y, int mask ){}
void HandleDestroy() { exit(0); }


double GetWindMaxPingTime( void )
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

void DrawMainText( const char * stbuf )
{
	int x, y;
	CNFGColor( 0x00 );
	for( x = -1; x < 2; x++ ) for( y = -1; y < 2; y++ )
	{
		CNFGPenX = 10+x; CNFGPenY = 10+y;
		CNFGDrawText( stbuf, 2 );
	}
	CNFGColor( 0xffffff );
	CNFGPenX = 10; CNFGPenY = 10;
	CNFGDrawText( stbuf, 2 );
}

void DrawFrameHistogram()
{
	int i;
//	double Now = OGGetAbsoluteTime();
	const int colwid = 50;
	int categories = (screenx-50)/colwid;
	int maxpingslot = ( globmaxtime*10000.0 );
	int minpingslot = ( globmintime*10000.0 );
	int slots = maxpingslot-minpingslot;

	if( categories <= 2 )
	{
		goto nodata;
	}
	else
	{
		int skips = ( (slots)/categories ) + 1;
		int slotsmax = maxpingslot / skips + 1;
		int slotsmin = minpingslot / skips;
		slots = slotsmax - slotsmin;
		if( slots <= 0 ) goto nodata;

		uint64_t samples[slots+2];
		int      ssmsMIN[slots+2];
		int      ssmsMAX[slots+2];
		int samp = minpingslot - 1;

		if( slots <= 1 ) goto nodata;

		memset( samples, 0, sizeof( samples ) );
		if( samp < 0 ) samp = 0;

		uint64_t highestchart = 0;
		int tslot = 0;
		for( i = slotsmin; i <= slotsmax; i++ )
		{
			int j;
			uint64_t total = 0;
			ssmsMIN[tslot] = samp;
			for( j = 0; j < skips; j++ )
			{
				total += hist_counts[samp++];
			}

			ssmsMAX[tslot] = samp;
			if( total > highestchart ) highestchart = total;
			samples[tslot++] = total;
		}

		if( highestchart <= 0 )
		{
			goto nodata;
		}

		int rslots = 0;
		for( i = 0; i < slots+1; i++ )
		{
			if( samples[i] ) rslots = i;
		}
		rslots++;

		for( i = 0; i < rslots; i++ )
		{
			CNFGColor( 0x33cc33 );
			int top = 30;
			uint64_t samps = samples[i];
			int bottom = screeny - 50;
			int height = samps?(samps * (bottom-top) / highestchart + 1):0;
			int startx = (i+1) * (screenx-50) / rslots;
			int endx = (i+2) * (screenx-50) / rslots;

			if( !in_frame_mode )
			{
				CNFGTackRectangle( startx, bottom-height, endx, bottom + 1 );
				CNFGColor( 0x00 );
			}
			else
			{
				CNFGColor( 0x8080ff );
			}
			CNFGTackSegment( startx, bottom+1, endx, bottom+1 );

			CNFGTackSegment( startx, bottom-height, startx, bottom+1 );
			CNFGTackSegment( endx,   bottom-height, endx,   bottom+1 );

			CNFGTackSegment( startx, bottom-height, endx, bottom-height );
			char stbuf[1024];
			int log10 = 1;
			int64_t ll = samps;
			while( ll >= 10 )
			{
				ll /= 10;
				log10++;
			}

			if( !in_frame_mode )
			{
				CNFGColor( 0xffffff );
			}
			else
			{
				CNFGColor( 0x8080ff );
			}


			CNFGPenX = startx + (8-log10) * 4; CNFGPenY = bottom+3;
#ifdef WIN32
			sprintf( stbuf, "%I64u", samps );
#else
			sprintf( stbuf, "%lu", samps );
#endif
			CNFGDrawText( stbuf, 2 );

			CNFGPenX = startx; CNFGPenY = bottom+14;
			sprintf( stbuf, "%5.1fms\n%5.1fms", ssmsMIN[i]/10.0, ssmsMAX[i]/10.0 );
			CNFGDrawText( stbuf, 2 );
		}
		char stt[1024];
#ifdef WIN32
		snprintf( stt, 1024, "Host: %s\nHistorical max  %9.2fms\nBiggest interval%9.2fms\nHistorical packet loss %I64u/%I64u = %6.3f%%",
#else
		snprintf( stt, 1024, "Host: %s\nHistorical max  %9.2fms\nBiggest interval%9.2fms\nHistorical packet loss %lu/%lu = %6.3f%%",
#endif
			pinghost, globmaxtime*1000.0, globinterval*1000.0, globallost, globalrx, globallost*100.0/(globalrx+globallost) );
		if( !in_frame_mode )
			DrawMainText( stt );
		return;
	}
nodata:
	DrawMainText( "No data.\n" );
	return;
}


void DrawFrame( void )
{
	int i;

	double now = OGGetAbsoluteTime();

	double totaltime = 0;
	int totalcountok = 0;
	int totalcountloss = 0;
	double mintime = 10000;
	double maxtime = 0;
	double stddev = 0;
	double last = -1;
	double loss = 100.00;
	double windmaxtime = GetWindMaxPingTime();

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
			if( i > 5 ) totalcountloss++; //Get a freebie on the first 5.
		}
		else // no ping sent for this point in time (after startup)
		{
			CNFGColor( 0x0 );
			dt = 99 * 1000; // assume 99s to fill screen black
		}

		if (!GuiYscaleFactorIsConstant)
		{
			GuiYScaleFactor =  (screeny - 50) / windmaxtime;
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

	char stbuf[2048];
	char * sptr = &stbuf[0];

	sptr += sprintf( sptr, 
		"Last:%6.2f ms    Host: %s\n"
		"Min :%6.2f ms\n"
		"Max :%6.2f ms    Historical max:   %5.2f ms\n"
		"Avg :%6.2f ms    Biggest interval: %5.2f ms\n"
#ifdef WIN32
		"Std :%6.2f ms    Historical loss:  %I64u/%I64u %5.3f%%\n"
#else
		"Std :%6.2f ms    Historical loss:  %lu/%lu %5.3f%%\n"
#endif
		"Loss:%6.1f %%", last, pinghost, mintime, maxtime, globmaxtime*1000, avg, globinterval*1000.0, stddev,
		globallost, globalrx, globallost*100.0f/(globalrx+globallost), loss );

	DrawMainText( stbuf );
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
int main( int argc, const char ** argv )
{
	char title[1024];
	int i;
	double ThisTime;
	double LastFPSTime = OGGetAbsoluteTime();
	double LastFrameTime = OGGetAbsoluteTime();
	double SecToWait;
	double frameperiodseconds;

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
#endif
	pingperiodseconds = 0.02;
	ExtraPingSize = 0;
	title[0] = 0;
	GuiYScaleFactor = 0;
  
	//We need to process all the unmarked parameters.
	int argcunmarked = 1;
	int displayhelp = 0;

	for( i = 1; i < argc; i++ )
	{
		const char * thisargv = argv[i];
		if( thisargv[0] == '-' )
		{
			int np = ++i;
			if( np >= argc )
			{
				displayhelp = 1;
				break;
			}
			const char * nextargv = argv[np];
			//Parameter-based field.
			switch( thisargv[1] )
			{
				case 'h': pinghost = nextargv; break;
				case 'p': pingperiodseconds = atof( nextargv ); break;
				case 's': ExtraPingSize = atoi( nextargv ); break;
				case 'y': GuiYScaleFactor = atof( nextargv ); break;
				case 't': sprintf(title, "%s", nextargv); break;
				case 'm': in_histogram_mode = 1; break;
				default: displayhelp = 1; break;
			}
		}
		else
		{
			//Unmarked fields
			switch( argcunmarked++ )
			{
				case 1: pinghost = thisargv; break;
				case 2: pingperiodseconds = atof( thisargv ); break;
				case 3: ExtraPingSize = atoi( thisargv ); break;
				case 4: GuiYScaleFactor = atof( thisargv ); break;
				case 5: sprintf(title, "%s", thisargv ); break;
				default: displayhelp = 1;
			}
		}
	}

	if( title[0] == 0 )
	{
		sprintf( title, "%s - cnping", pinghost );
	}

	if( GuiYScaleFactor > 0 )
	{
		GuiYscaleFactorIsConstant = 1;
	}

	if( !pinghost )
	{
		displayhelp = 1;
	}

	if( displayhelp )
	{
		#ifdef WIN32
		ERRM( "Need at least a host address to ping.\n" );
		#else
		ERRM( "Usage: cnping [host] [period] [extra size] [y-axis scaling] [window title]\n"
			"   (-h) [host]                 -- domain, IP address of ping target for ICMP or http host, i.e. http://google.com\n"
			"   (-p) [period]               -- period in seconds (optional), default 0.02 \n"
			"   (-s) [extra size]           -- ping packet extra size (above 12), optional, default = 0 \n"
			"   (-y) [const y-axis scaling] -- use a fixed scaling factor instead of auto scaling (optional)\n"
			"   (-t) [window title]         -- the title of the window (optional)\n");
		#endif
		return -1;
	}
 
	CNFGSetup( title, 320, 155 );

	if( memcmp( pinghost, "http://", 7 ) == 0 )
	{
		StartHTTPing( pinghost+7, pingperiodseconds );
	}
	else
	{
		ping_setup();
		OGCreateThread( PingSend, 0 );
		OGCreateThread( PingListen, 0 );
	}


	frameperiodseconds = fmin(.2, fmax(.03, pingperiodseconds));

	while(1)
	{
		iframeno++;
		CNFGHandleInput();

		CNFGClearFrame();
		CNFGColor( 0xFFFFFF );
		CNFGGetDimensions( &screenx, &screeny );

		if( in_frame_mode )
		{
			DrawFrame();
		}

		if( in_histogram_mode )
		{
			DrawFrameHistogram();
		}

		CNFGPenX = 100; CNFGPenY = 100;
		CNFGColor( 0xff );
		if( ping_failed_to_send )
		{
			CNFGDrawText( "Could not send ping.\nIs target reachable?\nDo you have sock_raw to privileges?", 3 );
		}
		else
		{
			CNFGDrawText( errbuffer, 3 );
		}


		frames++;
		CNFGSwapBuffers();

		ThisTime = OGGetAbsoluteTime();
		if( ThisTime > LastFPSTime + 1 )
		{
			frames = 0;
			LastFPSTime+=1;
		}

		SecToWait = frameperiodseconds - ( ThisTime - LastFrameTime );
		LastFrameTime += frameperiodseconds;
		//printf("iframeno = %d; SecToWait = %f; pingperiodseconds = %f; frameperiodseconds = %f \n", iframeno, SecToWait, pingperiodseconds, frameperiodseconds);
		if( SecToWait > 0 )
			OGUSleep( (int)( SecToWait * 1000000 ) );
	}

	return(0);
}

