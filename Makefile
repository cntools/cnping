all : cnping searchnet cnping-mousey

CFLAGS:=$(CFLAGS) -g -Os -I/opt/X11/include
CXXFLAGS:=$(CFLAGS)
LDFLAGS:=-g -L/opt/X11/lib/

MINGW32:=/usr/bin/i686-w64-mingw32-

cnping.exe : cnping.c DrawFunctions.c WinDriver.c os_generic.c ping.c
	$(MINGW32)windres resources.rc -o resources.o
	$(MINGW32)gcc -g -mwindows -m32 $(CFLAGS) resources.o -o $@ $^  -lgdi32 -lws2_32 -s

cnping : cnping.o DrawFunctions.o XDriver.o os_generic.o ping.o
	gcc $(CFLAGS) -o $@ $^ -lX11 -lm -lpthread -lXinerama -lXext $(LDFLAGS) 

cnping-mousey : cnping-mousey.o DrawFunctions.o XDriver.o os_generic.o ping.o
	gcc $(CFLAGS) -o $@ $^ -lX11 -lm -lpthread -lXinerama -lXext $(LDFLAGS) 

searchnet : os_generic.o ping.o searchnet.o
	gcc $(CFLAGS) -o $@ $^ -lpthread

linuxinstall : cnping
	sudo cp cnping /usr/local/bin/
	sudo chmod +s /usr/local/bin/cnping

clean : 
	rm -rf *.o *~ cnping cnping.exe cnping-mousey searchnet

