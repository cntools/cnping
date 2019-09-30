all : cnping searchnet

CFLAGS?=-s -Os -I/opt/X11/include -Wall
LDFLAGS?=-s -L/opt/X11/lib/
CC?=gcc

#CFLAGS:=$(CFLAGS) -DCNFGOGL
#LDFLAGS:=$(LDFLAGS) -lGL

#MINGW32:=/usr/bin/i686-w64-mingw32-
MINGW32?=i686-w64-mingw32-

#If you don't need admin priveleges
ADMINFLAGS:= $(ADMINFLAGS) -DWIN_USE_NO_ADMIN_PING

cnping-wingdi.exe : cnping.c rawdraw/CNFGFunctions.c rawdraw/CNFGWinDriver.c ping.c httping.c
	$(MINGW32)windres resources.rc -o resources.o $(ADMINFLAGS)
	$(MINGW32)gcc -g -fno-ident -mwindows -m32 $(CFLAGS) resources.o -o $@ $^  -lgdi32 -lws2_32 -s -D_WIN32_WINNT=0x0600 -DWIN32 -liphlpapi -DMINGW_BUILD $(ADMINFLAGS)

cnping.exe : cnping.c rawdraw/CNFGFunctions.c rawdraw/CNFGWinDriver.c ping.c httping.c
	$(MINGW32)windres resources.rc -o resources.o $(ADMINFLAGS)
	$(MINGW32)gcc -g -fno-ident -mwindows -m32 -DCNFGOGL $(CFLAGS)  resources.o -o $@ $^  -lgdi32 -lws2_32 -s -D_WIN32_WINNT=0x0600 -DWIN32 -liphlpapi -lopengl32 -DMINGW_BUILD $(ADMINFLAGS)

cnping : cnping.o rawdraw/CNFGFunctions.o rawdraw/CNFGXDriver.o ping.o httping.o
	$(CC) $(CFLAGS) -o $@ $^ -lX11 -lm -lpthread $(LDFLAGS)

cnping_ogl : cnping.c rawdraw/CNFGFunctions.c rawdraw/CNFGXDriver.c ping.c httping.c
	$(CC) $(CFLAGS) -o $@ $^ -DCNFGOGL $(CFLAGS) -s -lX11 -lm -lpthread $(LDFLAGS) -lGL

cnping_mac : cnping.c rawdraw/CNFGFunctions.c rawdraw/CNFGCocoaCGDriver.m ping.c httping.o
	$(CC) -o cnping $^ -x objective-c -framework Cocoa -framework QuartzCore -lm -lpthread

searchnet : ping.o searchnet.o
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

linuxinstall : cnping
	sudo rm -f /usr/local/bin/cnping
	sudo cp cnping /usr/local/bin/
	sudo setcap cap_net_raw+ep /usr/local/bin/cnping
#	sudo chmod +t /usr/local/bin/cnping  #One option - set the stuid bit.
#	sudo install cnping /usr/local/bin/  #Another option - using install.

clean : 
	rm -rf *.o *~ cnping cnping.exe cnping_mac searchnet
	rm -rf rawdraw/*.o
