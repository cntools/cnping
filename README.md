cnping
======

Minimal Graphical IPV4 Ping Tool.  (also comes with searchnet, like nmap but smaller and simpler).  It uses rawdraw so it is OS independent.
```
Usage: cnping [OPTION...] HOST

cnping -- Minimal Graphical IPV4 Ping Tool.

  -p, --period=PERIOD        Period in seconds (optional), default 0.02
  -x, --extra-size=SIZE      Ping packet extra size (above 12), optional,
                             default = 0
  -s, --scale=SCALE          Use fixed scaling factor instead of auto scaling
  -t, --title=TITLE          The window title
  -?, --help                 Give this help list
      --usage                Give a short usage message
  -V, --version              Print program version

Mandatory or optional arguments to long options are also mandatory or optional
for any corresponding short options.

Report bugs to https://github.com/cnlohr/cnping.
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
