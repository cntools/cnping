all : cnping searchnet cnping-mousey

CFLAGS:=$(CFLAGS) -g -Os -I/opt/X11/include -Wall
CXXFLAGS:=$(CFLAGS)
LDFLAGS:=-g -L/opt/X11/lib/

#CFLAGS:=$(CFLAGS) -DCNFGOGL
#LDFLAGS:=$(LDFLAGS) -lGL

#MINGW32:=/usr/bin/i686-w64-mingw32-
MINGW32:=i686-w64-mingw32-

cnping.exe : cnping.c CNFGFunctions.c CNFGWinDriver.c os_generic.c ping.c httping.c
	$(MINGW32)windres resources.rc -o resources.o
	$(MINGW32)gcc -g -mwindows -m32 $(CFLAGS) resources.o -o $@ $^  -lgdi32 -lws2_32 -s -D_WIN32_WINNT=0x0600 -DWIN32

cnping : cnping.o CNFGFunctions.o CNFGXDriver.o os_generic.o ping.o httping.o
	gcc $(CFLAGS) -o $@ $^ -lX11 -lm -lpthread $(LDFLAGS) 

cnping-mousey : cnping-mousey.o CNFGFunctions.o CNFGXDriver.o os_generic.o ping.o
	gcc $(CFLAGS) -o $@ $^ -lX11 -lm -lpthread $(LDFLAGS) 

cnping_mac : cnping.c CNFGFunctions.c CNFGCocoaCGDriver.m os_generic.c ping.c
	gcc -o cnping $^ -x objective-c -framework Cocoa -framework QuartzCore -lm -lpthread

cnping-mousey_mac : cnping-mousey.c CNFGFunctions.c CNFGCocoaCGDriver.m os_generic.c ping.c
	gcc -o cnping-mousey $^ -x objective-c -framework Cocoa -framework QuartzCore -lm -lpthread

searchnet : os_generic.o ping.o searchnet.o
	gcc $(CFLAGS) -o $@ $^ -lpthread

linuxinstall : cnping
	sudo rm -f /usr/local/bin/cnping
	sudo cp cnping /usr/local/bin/
	sudo setcap cap_net_raw+ep /usr/local/bin/cnping
#	sudo chmod +t /usr/local/bin/cnping  #One option - set the stuid bit.
#	sudo install cnping /usr/local/bin/  #Another option - using install.

clean : 
	rm -rf *.o *~ cnping cnping.exe cnping-mousey searchnet

