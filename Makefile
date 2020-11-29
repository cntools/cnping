CFLAGS?=-s -Os -I/opt/X11/include -Irdui -Wall
LDFLAGS?=-s -L/opt/X11/lib/
CC?=gcc

all : cnping

#CFLAGS:=$(CFLAGS) -DCNFGOGL
#LDFLAGS:=$(LDFLAGS) -lGL

clean :
	rm -rf *.o *~ cnping cnping.exe cnping_mac searchnet
	rm -rf rawdraw/*.o


# Windows

#MINGW32:=/usr/bin/i686-w64-mingw32-
MINGW32?=i686-w64-mingw32-

# If you don't need admin privileges
ADMINFLAGS:= $(ADMINFLAGS) -DWIN_USE_NO_ADMIN_PING

cnping-wingdi.exe : cnping.c ping.c httping.c resources.o
	$(MINGW32)gcc -g -fno-ident -mwindows -m32 $(CFLAGS) -o $@ $^  -lgdi32 -lws2_32 -s -D_WIN32_WINNT=0x0600 -DWIN32 -liphlpapi -DMINGW_BUILD $(ADMINFLAGS)

cnping.exe : cnping.c ping.c httping.c resources.o
	$(MINGW32)gcc -g -fno-ident -mwindows -m32 -DCNFGOGL $(CFLAGS) -o $@ $^  -lgdi32 -lws2_32 -s -D_WIN32_WINNT=0x0600 -DWIN32 -liphlpapi -lopengl32 -DMINGW_BUILD $(ADMINFLAGS)

resources.o : resources.rc
	$(MINGW32)windres resources.rc -o resources.o $(ADMINFLAGS)


# Unix

cnping : cnping.c ping.c httping.c
	$(CC) $(CFLAGS) -o $@ $^ -lX11 -lm -lpthread -lGL $(LDFLAGS)

cnping_ogl : cnping.c ping.c httping.c
	$(CC) $(CFLAGS) -o $@ $^ -DCNFGOGL $(CFLAGS) -s -lX11 -lm -lpthread $(LDFLAGS) -lGL

cnping_mac : cnping.c ping.c httping.c
	$(CC) -o cnping $^ -x objective-c -framework Cocoa -framework QuartzCore -lm -lpthread -DOSX

searchnet : ping.o searchnet.o
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

linuxinstall : cnping
	sudo rm -f /usr/local/bin/cnping
	sudo cp cnping /usr/local/bin/
	sudo setcap cap_net_raw+ep /usr/local/bin/cnping
#	sudo chmod +t /usr/local/bin/cnping  #One option - set the stuid bit.
#	sudo install cnping /usr/local/bin/  #Another option - using install.

