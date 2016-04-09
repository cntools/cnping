all : cnping searchnet cnping-mousey

CFLAGS:=$(CFLAGS) -g -Os
CXXFLAGS:=$(CFLAGS)
LDFLAGS:=-g

MINGW32:=/usr/bin/i686-w64-mingw32-

cnping.exe : cnping.c DrawFunctions.c WinDriver.c os_generic.c ping.c
	$(MINGW32)gcc -g -mwindows -m32 $(CFLAGS) -o $@ $^  -lgdi32 -lws2_32 -s

cnping : cnping.o DrawFunctions.o XDriver.o os_generic.o ping.o
	gcc $(CFLAGS) -o $@ $^ -lX11 -lm -lpthread -lXinerama -lXext $(LDFLAGS) 

cnping-mousey : cnping-mousey.o DrawFunctions.o XDriver.o os_generic.o ping.o
	gcc $(CFLAGS) -o $@ $^ -lX11 -lm -lpthread -lXinerama -lXext $(LDFLAGS) 

searchnet : os_generic.o ping.o searchnet.o
	gcc $(CFLAGS) -o $@ $^ -lpthread
 
clean : 
	rm -rf *.o *~ cnping cnping.exe

