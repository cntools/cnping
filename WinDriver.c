//Copyright (c) 2011 <>< Charles Lohr - Under the MIT/x11 or NewBSD License you choose.
//Portion from: http://en.wikibooks.org/wiki/Windows_Programming/Window_Creation


#include "DrawFunctions.h"
#include <windows.h>
#include <stdlib.h>
#include <malloc.h> //for alloca

static HBITMAP lsBitmap;
static HINSTANCE lhInstance;
static HWND lsHWND;
static HDC lsWindowHDC;
static HDC lsHDC;
static unsigned int bufferx;
static unsigned int buffery;
static uint32_t * buffer = 0;

void CNFGClearFrame()
{
	uint32_t col = CNFGColor( CNFGBGColor );
	int i;
	for( i = bufferx * buffery - 1; i>=0; i-- )
	{
		buffer[i] = col;
	}
}

void InternalHandleResize()
{
	if( lsBitmap ) DeleteObject( lsBitmap );

	if( buffer ) free( buffer );
	buffer = malloc( bufferx * buffery * 4 );

	lsBitmap = CreateBitmap( bufferx, buffery, 1, 32, buffer );
	SelectObject( lsHDC, lsBitmap );
}

void CNFGSwapBuffers()
{
	int thisw, thish;
	RECT r;

	int a = SetBitmapBits(lsBitmap,bufferx*buffery*4,buffer);
	a = BitBlt(lsWindowHDC, 0, 0, bufferx, buffery, lsHDC, 0, 0, SRCCOPY);
	UpdateWindow( lsHWND );


	//Check to see if the window is closed.
	if( !IsWindow( lsHWND ) )
	{
		exit( 0 );
	}

	GetClientRect( lsHWND, &r );
	thisw = r.right - r.left;
	thish = r.bottom - r.top;
	if( thisw != bufferx || thish != buffery )
	{
		bufferx = thisw;
		buffery = thish;
		InternalHandleResize();
	}
}


static uint32_t SWAPS( uint32_t r )
{
	uint32_t ret = (r&0xFF)<<16;
	r>>=8;
	ret |= (r&0xff)<<8;
	r>>=8;
	ret |= r;
	return ret;
}

uint32_t CNFGColor( uint32_t RGB )
{
	CNFGLastColor = SWAPS(RGB);
	return CNFGLastColor;
}

void CNFGTackPixel( short tx, short ty )
{
	buffer[ty * bufferx + tx] = CNFGLastColor;
}

void CNFGTackSegment( short x1, short y1, short x2, short y2 )
{
	short tx, ty;
	float slope, lp;

	short dx = x2 - x1;
	short dy = y2 - y1;

	if( !buffer ) return;

	if( dx < 0 ) dx = -dx;
	if( dy < 0 ) dy = -dy;

	if( dx > dy )
	{
		short minx = (x1 < x2)?x1:x2;
		short maxx = (x1 < x2)?x2:x1;
		short miny = (x1 < x2)?y1:y2;
		short maxy = (x1 < x2)?y2:y1;
		float thisy = miny;
		slope = (float)(maxy-miny) / (float)(maxx-minx);

		for( tx = minx; tx <= maxx; tx++ )
		{
			ty = thisy;
			if( tx < 0 || ty < 0 || ty >= buffery ) continue;
			if( tx >= bufferx ) break;
			buffer[ty * bufferx + tx] = CNFGLastColor;
			thisy += slope;
		}
	}
	else
	{
		short minx = (y1 < y2)?x1:x2;
		short maxx = (y1 < y2)?x2:x1;
		short miny = (y1 < y2)?y1:y2;
		short maxy = (y1 < y2)?y2:y1;
		float thisx = minx;
		slope = (float)(maxx-minx) / (float)(maxy-miny);

		for( ty = miny; ty <= maxy; ty++ )
		{
			tx = thisx;
			if( ty < 0 || tx < 0 || tx >= bufferx ) continue;
			if( ty >= buffery ) break;
			buffer[ty * bufferx + tx] = CNFGLastColor;
			thisx += slope;
		}
	}
}

void CNFGTackRectangle( short x1, short y1, short x2, short y2 )
{
	short minx = (x1<x2)?x1:x2;
	short miny = (y1<y2)?y1:y2;
	short maxx = (x1>=x2)?x1:x2;
	short maxy = (y1>=y2)?y1:y2;

	short x, y;

	if( minx < 0 ) minx = 0;
	if( miny < 0 ) miny = 0;
	if( maxx >= bufferx ) maxx = bufferx-1;
	if( maxy >= buffery ) maxy = buffery-1;

	for( y = miny; y <= maxy; y++ )
	{
		uint32_t * bufferstart = &buffer[y * bufferx + minx];
		for( x = minx; x <= maxx; x++ )
		{
			(*bufferstart++) = CNFGLastColor;
		}
	}
}

void CNFGTackPoly( RDPoint * points, int verts )
{
	short minx = 10000, miny = 10000;
	short maxx =-10000, maxy =-10000;
	short i, x, y;

	//Just in case...
	if( verts > 32767 ) return;

	for( i = 0; i < verts; i++ )
	{
		RDPoint * p = &points[i];
		if( p->x < minx ) minx = p->x;
		if( p->y < miny ) miny = p->y;
		if( p->x > maxx ) maxx = p->x;
		if( p->y > maxy ) maxy = p->y;
	}

	if( miny < 0 ) miny = 0;
	if( maxy >= buffery ) maxy = buffery-1;

	for( y = miny; y <= maxy; y++ )
	{
		short startfillx = maxx;
		short endfillx = minx;

		//Figure out what line segments intersect this line.
		for( i = 0; i < verts; i++ )
		{
			short pl = i + 1;
			if( pl == verts ) pl = 0;

			RDPoint ptop;
			RDPoint pbot;

			ptop.x = points[i].x;
			ptop.y = points[i].y;
			pbot.x = points[pl].x;
			pbot.y = points[pl].y;
//printf( "Poly: %d %d\n", pbot.y, ptop.y );

			if( pbot.y < ptop.y )
			{
				RDPoint ptmp;
				ptmp.x = pbot.x;
				ptmp.y = pbot.y;
				pbot.x = ptop.x;
				pbot.y = ptop.y;
				ptop.x = ptmp.x;
				ptop.y = ptmp.y;
			}

			//Make sure this line segment is within our range.
//printf( "PT: %d %d %d\n", y, ptop.y, pbot.y );
			if( ptop.y <= y && pbot.y >= y )
			{
				short diffy = pbot.y - ptop.y;
				uint32_t placey = (uint32_t)(y - ptop.y)<<16;  //Scale by 16 so we can do integer math.
				short diffx = pbot.x - ptop.x;
				short isectx;

				if( diffy == 0 )
				{
					if( pbot.x < ptop.x )
					{
						if( startfillx > pbot.x ) startfillx = pbot.x;
						if( endfillx < ptop.x ) endfillx = ptop.x;
					}
					else
					{
						if( startfillx > ptop.x ) startfillx = ptop.x;
						if( endfillx < pbot.x ) endfillx = pbot.x;
					}
				}
				else
				{
					//Inner part is scaled by 65536, outer part must be scaled back.
					isectx = (( (placey / diffy) * diffx + 32768 )>>16) + ptop.x;
					if( isectx < startfillx ) startfillx = isectx;
					if( isectx > endfillx ) endfillx = isectx;
				}
//printf( "R: %d %d %d\n", pbot.x, ptop.x, isectx );
			}
		}

//printf( "%d %d %d\n", y, startfillx, endfillx );

		if( endfillx >= bufferx ) endfillx = bufferx - 1;
		if( endfillx >= bufferx ) endfillx = buffery - 1;
		if( startfillx < 0 ) startfillx = 0;
		if( startfillx < 0 ) startfillx = 0;

		unsigned int * bufferstart = &buffer[y * bufferx + startfillx];
		for( x = startfillx; x <= endfillx; x++ )
		{
			(*bufferstart++) = CNFGLastColor;
		}
	}
//exit(1);
}



void CNFGGetDimensions( short * x, short * y )
{
	*x = bufferx;
	*y = buffery;
}

//This was from the article
LRESULT CALLBACK MyWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
	{
	case WM_DESTROY:
		CNFGTearDown();
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void CNFGTearDown()
{
	PostQuitMessage(0);
}

//This was from the article, too... well, mostly.
void CNFGSetup( const char * name_of_window, int width, int height )
{
	static LPSTR szClassName = "MyClass";
	RECT client, window;
	WNDCLASS wnd;
	int w, h, wd, hd;
	HINSTANCE hInstance = GetModuleHandle(NULL);

	bufferx = width;
	buffery = height;

	wnd.style = CS_HREDRAW | CS_VREDRAW; //we will explain this later
	wnd.lpfnWndProc = MyWndProc;
	wnd.cbClsExtra = 0;
	wnd.cbWndExtra = 0;
	wnd.hInstance = hInstance;
	wnd.hIcon = LoadIcon(NULL, IDI_APPLICATION); //default icon
	wnd.hCursor = LoadCursor(NULL, IDC_ARROW);   //default arrow mouse cursor
	wnd.hbrBackground = (HBRUSH)(COLOR_BACKGROUND);
	wnd.lpszMenuName = NULL;                     //no menu
	wnd.lpszClassName = szClassName;

	if(!RegisterClass(&wnd))                     //register the WNDCLASS
	{
		MessageBox(NULL, "This Program Requires Windows NT", "Error", MB_OK);
	}

	lsHWND = CreateWindow(szClassName,
		name_of_window,      //name_of_window,
		WS_OVERLAPPEDWINDOW, //basic window style
		CW_USEDEFAULT,
		CW_USEDEFAULT,       //set starting point to default value
		bufferx,
		buffery,        //set all the dimensions to default value
		NULL,                //no parent window
		NULL,                //no menu
		hInstance,
		NULL);               //no parameters to pass


	lsWindowHDC = GetDC( lsHWND );

	lsHDC = CreateCompatibleDC( lsWindowHDC );
	//lsBackBitmap = CreateCompatibleBitmap( lsWindowHDC, lsLastWidth, buffery );
	//SelectObject( lsHDC, lsBackBitmap );

	//lsClearBrush = CreateSolidBrush( CNFGBGColor );
	//lsHBR = CreateSolidBrush( 0xFFFFFF );
	//lsHPEN = CreatePen( PS_SOLID, 0, 0xFFFFFF );

	ShowWindow(lsHWND, 1);              //display the window on the screen

	//Once set up... we have to change the window's borders so we get the client size right.
	GetClientRect( lsHWND, &client );
	GetWindowRect( lsHWND, &window );
	w = ( window.right - window.left);
	h = ( window.bottom - window.top);
	wd = w - client.right;
	hd = h - client.bottom;
	MoveWindow( lsHWND, window.left, window.top, bufferx + wd, buffery + hd, 1 );

	InternalHandleResize();
}

void CNFGHandleInput()
{
	int ldown = 0;

	MSG msg;
	while( PeekMessage( &msg, lsHWND, 0, 0xFFFF, 1 ) )
	{
		TranslateMessage(&msg);

		switch( msg.message )
		{
		case WM_MOUSEMOVE:
			HandleMotion( (msg.lParam & 0xFFFF), (msg.lParam>>16) & 0xFFFF, ( (msg.wParam & 0x01)?1:0) | ((msg.wParam & 0x02)?2:0) | ((msg.wParam & 0x10)?4:0) );
			break;
		case WM_LBUTTONDOWN:	HandleButton( (msg.lParam & 0xFFFF), (msg.lParam>>16) & 0xFFFF, 1, 1 ); break;
		case WM_RBUTTONDOWN:	HandleButton( (msg.lParam & 0xFFFF), (msg.lParam>>16) & 0xFFFF, 2, 1 ); break;
		case WM_MBUTTONDOWN:	HandleButton( (msg.lParam & 0xFFFF), (msg.lParam>>16) & 0xFFFF, 3, 1 ); break;
		case WM_LBUTTONUP:		HandleButton( (msg.lParam & 0xFFFF), (msg.lParam>>16) & 0xFFFF, 1, 0 ); break;
		case WM_RBUTTONUP:		HandleButton( (msg.lParam & 0xFFFF), (msg.lParam>>16) & 0xFFFF, 2, 0 ); break;
		case WM_MBUTTONUP:		HandleButton( (msg.lParam & 0xFFFF), (msg.lParam>>16) & 0xFFFF, 3, 0 ); break;
		case WM_KEYDOWN:
		case WM_KEYUP:
			HandleKey( tolower( msg.wParam ), (msg.message==WM_KEYDOWN) );
			break;
		default:
			DispatchMessage(&msg);
			break;
		}
	}
}


/*
static HBITMAP lsBackBitmap;
static HDC lsWindowHDC;
static HBRUSH lsHBR;
static HPEN lsHPEN;
static HBRUSH lsClearBrush;

static void InternalHandleResize()
{
	DeleteObject( lsBackBitmap );
	lsBackBitmap = CreateCompatibleBitmap( lsHDC, lsLastWidth, lsLastHeight );
	SelectObject( lsHDC, lsBackBitmap );

}

uint32_t CNFGColor( uint32_t RGB )
{
	CNFGLastColor = RGB;

	DeleteObject( lsHBR );
	lsHBR = CreateSolidBrush( RGB );
	SelectObject( lsHDC, lsHBR );

	DeleteObject( lsHPEN );
	lsHPEN = CreatePen( PS_SOLID, 0, RGB );
	SelectObject( lsHDC, lsHPEN );

	return RGB;
}

void CNFGTackSegment( short x1, short y1, short x2, short y2 )
{
	POINT pt[2] = { {x1, y1}, {x2, y2} };
	Polyline( lsHDC, pt, 2 );
	SetPixel( lsHDC, x1, y1, CNFGLastColor );
	SetPixel( lsHDC, x2, y2, CNFGLastColor );
}

void CNFGTackRectangle( short x1, short y1, short x2, short y2 )
{
	RECT r;
	if( x1 < x2 ) { r.left = x1; r.right = x2; }
	else          { r.left = x2; r.right = x1; }
	if( y1 < y2 ) { r.top = y1; r.bottom = y2; }
	else          { r.top = y2; r.bottom = y1; }
	FillRect( lsHDC, &r, lsHBR );
}

void CNFGClearFrame()
{
	RECT r = { 0, 0, lsLastWidth, lsLastHeight };
	DeleteObject( lsClearBrush  );
	lsClearBrush = CreateSolidBrush( CNFGBGColor );
	SelectObject( lsHDC, lsClearBrush );

	FillRect( lsHDC, &r, lsClearBrush );
}

void CNFGSwapBuffers()
{
	int thisw, thish;
	RECT r;
	BitBlt( lsWindowHDC, 0, 0, lsLastWidth, lsLastHeight, lsHDC, 0, 0, SRCCOPY );
	UpdateWindow( lsHWND );

	//Check to see if the window is closed.
	if( !IsWindow( lsHWND ) )
	{
		exit( 0 );
	}

	GetClientRect( lsHWND, &r );
	thisw = r.right - r.left;
	thish = r.bottom - r.top;
	if( thisw != lsLastWidth || thish != lsLastHeight )
	{
		lsLastWidth = thisw;
		lsLastHeight = thish;
		InternalHandleResize();
	}
}

void CNFGTackPoly( RDPoint * points, int verts )
{
	int i;
	POINT * t = (POINT*)alloca( sizeof( POINT ) * verts );
	for( i = 0; i < verts; i++ )
	{
		t[i].x = points[i].x;
		t[i].y = points[i].y;
	}
	Polygon( lsHDC, t, verts );
}


void CNFGTackPixel( short x1, short y1 )
{
	SetPixel( lsHDC, x1, y1, CNFGLastColor );
}
*/

