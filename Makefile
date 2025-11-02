CFLAGS ?= -s -Os -I/opt/X11/include -Wall
LDFLAGS ?= -s -L/opt/X11/lib/
CC? = gcc

ICONSPATH = freedesktop/icons/hicolor/
APPNAME = com.github.cntools.cnping

all : cnping

#CFLAGS := $(CFLAGS) -DCNFGOGL
#LDFLAGS := $(LDFLAGS) -lGL

clean :
	rm -rf *.o *~ cnping cnping.exe cnping_mac searchnet
	rm -rf rawdraw/*.o


# Windows

#MINGW32 := /usr/bin/i686-w64-mingw32-
MINGW32 ?= i686-w64-mingw32-

# If you don't need admin privileges
ADMINFLAGS := $(ADMINFLAGS) -DWIN_USE_NO_ADMIN_PING

# Add git version to CFLAGS
GIT_VERSION := "$(shell git describe --abbrev=4 --dirty --always --tags)"
CFLAGS += -DVERSION=\"$(GIT_VERSION)\"

cnping-wingdi.exe : cnping.c ping.c httping.c pinghostlist.c resolve.c resources.o
	$(MINGW32)gcc -g -fno-ident -mwindows -m32 $(CFLAGS) -o $@ $^  -lgdi32 -lws2_32 -s -D_WIN32_WINNT=0x0600 -DWIN32 -liphlpapi -DMINGW_BUILD $(ADMINFLAGS)

cnping.exe : cnping.c ping.c httping.c pinghostlist.c resolve.c resources.o
	$(MINGW32)gcc -g -fno-ident -mwindows -m32 -DCNFGOGL $(CFLAGS) -o $@ $^  -lgdi32 -lws2_32 -s -D_WIN32_WINNT=0x0600 -DWIN32 -liphlpapi -lopengl32 -DMINGW_BUILD $(ADMINFLAGS)

resources.o : resources.rc
	$(MINGW32)windres resources.rc -o resources.o $(ADMINFLAGS)


# Unix

cnping : cnping.c ping.c httping.c resolve.c pinghostlist.c
	$(CC) $(CFLAGS) -o $@ $^ -lX11 -lm -lpthread -lGL $(LDFLAGS)

cnping_ogl : cnping.c ping.c httping.c resolve.c pinghostlist.c
	$(CC) $(CFLAGS) -o $@ $^ -DCNFGOGL $(CFLAGS) -lX11 -lm -lpthread $(LDFLAGS) -lGL

cnping_mac : cnping.c ping.c httping.c resolve.c pinghostlist.c
	$(CC) -o cnping $^ -x objective-c -framework Cocoa -framework QuartzCore -lm -lpthread -DOSX

searchnet : ping.c searchnet.c resolve.c
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

linuxinstall : cnping
	rm -f /usr/local/bin/cnping
	cp cnping /usr/local/bin/
	cp -r $(ICONSPATH) /usr/local/share/icons
	cp freedesktop/${APPNAME}.desktop /usr/local/share/applications
	cp freedesktop/${APPNAME}.metainfo.xml /usr/local/share/metainfo
	setcap cap_net_raw+ep /usr/local/bin/cnping
#	sudo chmod +t /usr/local/bin/cnping  #One option - set the stuid bit.
#	sudo install cnping /usr/local/bin/  #Another option - using install.

# minimal linux install, may be useful for development
minlinuxinstall : cnping
	sudo rm -f /usr/local/bin/cnping
	sudo cp cnping /usr/local/bin/
	sudo setcap cap_net_raw+ep /usr/local/bin/cnping



# this target requires imagemagick
updateicons : ${ICONSPATH}scalable/apps/${APPNAME}.svg
	convert $^ -resize 16x16 ${ICONSPATH}16x16/apps/${APPNAME}.png
	convert $^ -resize 32x32 ${ICONSPATH}32x32/apps/${APPNAME}.png
	convert $^ -resize 48x48 ${ICONSPATH}48x48/apps/${APPNAME}.png
	convert $^ -resize 256x256 ${ICONSPATH}256x256/apps/${APPNAME}.png
	convert $^ -resize 1024x1024 ${ICONSPATH}1024x1024/apps/${APPNAME}.png

# after creating the ico file use GIMP to compress it:
# Image-> Mode -> Indexed...
# Choose "Generate optimum palette"
# Maximum number of colors: 3 (may change if the icon changes)
# "Convert"
# File -> Export As
# Check "Compressed (PNG)" in every resolution
# "Export"
cnping.ico: ${ICONSPATH}scalable/apps/${APPNAME}.svg
	convert $^ -density 300 -define icon:auto-resize=256,64,16 -background none $@

