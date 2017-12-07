cnping
======

Minimal Graphical IPV4 Ping Tool.  (also comes with searchnet, like nmap but smaller and simpler).  It uses rawdraw so it is OS independent.
```
Usage: cnping [host] [period] [extra size] [y-axis scaling] [window title]

	 [host]                 -- domain or IP address of ping target
	 [period]               -- period in seconds (optional), default 0.02
	 [extra size]           -- ping packet extra size (above 12), optional, default = 0
	 [const y-axis scaling] -- use a fixed scaling factor instead of auto scaling (optional)
	 [window title]         -- the title of the window (optional)
```
Picture:

<IMG SRC=cnping.png>

## Installation:  

### Ubuntu:  

```
sudo apt install libxinerama-dev libxext-dev libx11-dev build-essential
make linuxinstall
```

'linuxinstall' builds the tool and copies it to your usr/local/bin folder, and sets the sticky bit allowing it to run as though it were root, allowing it to create raw sockets.

```
sudo cp cnping /usr/local/bin/
sudo chmod +s /usr/local/bin/cnping
```
### Archlinux:

 [cnping-git](https://aur.archlinux.org/packages/cnping-git/) in the [Arch User Repository](https://wiki.archlinux.org/index.php/Arch_User_Repository)
