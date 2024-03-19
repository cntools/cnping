#ifndef _HTTPING_H
#define _HTTPING_H

//Callbacks (when started/received)
void HTTPingCallbackStart( int seqno, unsigned int pingHostId );
void HTTPingCallbackGot( int seqno, unsigned int pingHostId );

//addy should be google.com/blah or something like that.  Do not include prefixing http://.  Port numbers OK.
int StartHTTPing( const char * addy, double minperiod, const char * device, unsigned int pingHostId );

#endif

