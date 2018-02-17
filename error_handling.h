#ifndef _ERROR_HANDLING
#define _ERROR_HANDLING

#ifdef WIN32

extern char errbuffer[1024];
#ifndef _MSC_VER
#define ERRM(x...) { sprintf( errbuffer, x ); MessageBox( 0, errbuffer, "cnping", 0 ); }
#else
#define ERRM(...) { sprintf( errbuffer, __VA_ARGS__ ); MessageBox( 0, errbuffer, "cnping", 0 ); }
#endif

#else

#define ERRM(x...) fprintf( stderr, x );

#endif


#endif
