all : cnping searchnet cnping-mousey

CFLAGS:=$(CFLAGS) -g -Os -I/opt/X11/include
CXXFLAGS:=$(CFLAGS)
LDFLAGS:=-g -L/opt/X11/lib/

#CFLAGS:=$(CFLAGS) -DCNFGOGL
#LDFLAGS:=$(LDFLAGS) -lGL

#MINGW32:=/usr/bin/i686-w64-mingw32-
MINGW32:=i686-w64-mingw32-

cnping.exe : cnping.c CNFGFunctions.c CNFGWinDriver.c os_generic.c ping.c
	$(MINGW32)windres resources.rc -o resources.o
	$(MINGW32)gcc -g -mwindows -m32 $(CFLAGS) resources.o -o $@ $^  -lgdi32 -lws2_32 -s -D_WIN32_WINNT=0x0600 -DWIN32

cnping : cnping.o CNFGFunctions.o CNFGXDriver.o os_generic.o ping.o
	gcc $(CFLAGS) -o $@ $^ -lX11 -lm -lpthread $(LDFLAGS) 

cnping-mousey : cnping-mousey.o CNFGFunctions.o CNFGXDriver.o os_generic.o ping.o
	gcc $(CFLAGS) -o $@ $^ -lX11 -lm -lpthread $(LDFLAGS) 

searchnet : os_generic.o ping.o searchnet.o
	gcc $(CFLAGS) -o $@ $^ -lpthread

linuxinstall : cnping
	sudo cp cnping /usr/local/bin/
	sudo chmod +s /usr/local/bin/cnping

clean : 
	rm -rf *.o *~ cnping cnping.exe cnping-mousey searchnet

