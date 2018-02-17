#ifndef _HTTPING_H
#define _HTTPING_H

//Callbacks (when started/received)
void HTTPingCallbackStart( int seqno );
void HTTPingCallbackGot( int seqno );

//addy should be google.com/blah or something like that.  Do not include prefixing http://.  Port numbers OK.
int StartHTTPing( const char * addy, double minperiod );

#endif

