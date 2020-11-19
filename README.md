cnping - Now with RDUI!
=======================

Minimal Graphical IPV4 Ping/HTTP Ping Tool.  (also comes with searchnet, like nmap but smaller and simpler).  It uses rawdraw so it is OS independent.
```
Usage: cnping [host] [period] [extra size] [y-axis scaling] [window title]

	 [host]                 -- domain or IP address of ICMP ping target, or http://[host] i.e. http://google.com
	 [period]               -- period in seconds (optional), default 0.02
	 [extra size]           -- ping packet extra size (above 12), optional, default = 0
	 [const y-axis scaling] -- use a fixed scaling factor instead of auto scaling (optional)
	 [window title]         -- the title of the window (optional)
```
Picture:

<IMG SRC=cnping.png>

If an http host is listed, the default request is ```HEAD /favicon.ico HTTP/1.1``` since this is usually a very fast, easy operation for the server.  If a specific file or uri is requested, that will be requested instead, i.e. http://github.com/cnlohr will request ```HEAD /cnlohr HTTP/1.1```.

If a regular hostname is requested instead, ICMP (regular ping) will be used.

This allows cnping to be operated in environments where ICMP is prohibited by local computer or network policies.

## Installation

### Ubuntu

```
sudo apt install libxinerama-dev libxext-dev libx11-dev build-essential mesa-common-dev libglvnd-dev
make linuxinstall
```

'linuxinstall' builds the tool, copies it to your /usr/local/bin folder, and sets the cap_net_raw capability allowing it to create raw sockets without being root.

```
sudo cp cnping /usr/local/bin/
sudo setcap cap_net_raw+ep /usr/local/bin/cnping
```

Note that if only http pinging is requested, you do not need cap_net_raw or root access.

### Archlinux

 [cnping-git](https://aur.archlinux.org/packages/cnping-git/) in the [Arch User Repository](https://wiki.archlinux.org/index.php/Arch_User_Repository)


### Windows

A Windows-exe can be cross compiled on Linux, just install the necessary dependencies and compile it:

```
sudo apt install binutils-mingw-w64-i686 gcc-mingw-w64-i686 g++-mingw-w64-i686
make cnping.exe
```
