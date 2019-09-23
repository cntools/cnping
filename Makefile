all : cnping searchnet

CFLAGS:=$(CFLAGS) -s -Os -I/opt/X11/include -Wall
CXXFLAGS:=$(CFLAGS)
LDFLAGS:=-s -L/opt/X11/lib/

#CFLAGS:=$(CFLAGS) -DCNFGOGL
#LDFLAGS:=$(LDFLAGS) -lGL

#MINGW32:=/usr/bin/i686-w64-mingw32-
MINGW32:=i686-w64-mingw32-

#If you don't need admin priveleges
ADMINFLAGS:= $(ADMINFLAGS) -DWIN_USE_NO_ADMIN_PING

cnping.exe : cnping.c rawdraw/CNFGFunctions.c rawdraw/CNFGWinDriver.c rawdraw/os_generic.c ping.c httping.c
	$(MINGW32)windres resources.rc -o resources.o $(ADMINFLAGS)
	$(MINGW32)gcc -g -fno-ident -mwindows -m32 $(CFLAGS) resources.o -o $@ $^  -lgdi32 -lws2_32 -s -D_WIN32_WINNT=0x0600 -DWIN32 -liphlpapi -DMINGW_BUILD $(ADMINFLAGS)

cnping : cnping.o rawdraw/CNFGFunctions.o rawdraw/CNFGXDriver.o rawdraw/os_generic.o ping.o httping.o
	gcc $(CFLAGS) -o $@ $^ -lX11 -lm -lpthread $(LDFLAGS) 

cnping_mac : cnping.c rawdraw/CNFGFunctions.c rawdraw/CNFGCocoaCGDriver.m rawdraw/os_generic.c ping.c httping.o
	gcc -o cnping $^ -x objective-c -framework Cocoa -framework QuartzCore -lm -lpthread

searchnet : rawdraw/os_generic.o ping.o searchnet.o
	gcc $(CFLAGS) -o $@ $^ -lpthread

linuxinstall : cnping
	sudo rm -f /usr/local/bin/cnping
	sudo cp cnping /usr/local/bin/
	sudo setcap cap_net_raw+ep /usr/local/bin/cnping
#	sudo chmod +t /usr/local/bin/cnping  #One option - set the stuid bit.
#	sudo install cnping /usr/local/bin/  #Another option - using install.

clean : 
	rm -rf *.o *~ cnping cnping.exe cnping_mac searchnet
	rm -rf rawdraw/*.o
