#ifndef _ERROR_HANDLING
#define _ERROR_HANDLING

extern char errbuffer[1024];

#ifdef WIN32

#ifndef _MSC_VER
#define ERRM(x...) { sprintf( errbuffer, x ); MessageBox( 0, errbuffer, "cnping", 0 ); }
#define ERRMB(x...) { sprintf( errbuffer, x ); }
#else
#define ERRM(...) { sprintf( errbuffer, __VA_ARGS__ ); MessageBox( 0, errbuffer, "cnping", 0 ); }
#define ERRMB(...) { sprintf( errbuffer, __VA_ARGS__ ); }
#endif

#else

#define ERRM(x...) { fprintf( stderr, x ); }
#define ERRMB(x...) { sprintf( errbuffer, x);  fprintf( stderr, x ); }

#endif


#endif
