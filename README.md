# ![GitHub Logo](/img/runns-logo.png)

## Contents  
* [About](#about)  
* [How to use it](#how-to-use-it)  
* [Troubleshooting](#troubleshooting)  
* [Example use-case](#example-use-case)  
* [Acknowledgement](#acknowledgement)  

## About
The project's goal is to conveniently start a program inside a [network
namespace](https://lwn.net/Articles/580893) without getting elevated, root,
privileges for the Linux based systems.  **RUNNS** consists of two pieces, a
daemon (server) called *runns* and a control utility (client) called *runnsctl*.

**Why would you need this?** Let's assume a simple situation - you have a VPN
with `0.0.0.0/1` default route (i.e. all of your traffic goes through VPN).
The VPN's bandwidth could be low and/or you don't want to pass some
connections through VPN, for example SSH to another machine, or you simply
don't want all of your traffic to be routed through VPN, or maybe, you want
to work with multiple VPNs. With *runns* it is fairly easy to isolate programs
inside a Linux network namespace to be used with specific VPN.

## How to use it
### Building
To build one need to have at least the following dependencies:
* [GNU make](https://www.gnu.org/software/make)
* C compiler, e.g. [GCC](https://gcc.gnu.org)
* libc, e.g. [glibc](https://www.gnu.org/software/libc)
* [binutils](https://www.gnu.org/software/binutils)
* [autoconf](https://www.gnu.org/software/autoconf)

The building procedure is straight forward:

* Run `autoconf` in the source directory to get the configure script: `$ autoconf`.
* Configure: `$ ./configure`.
* Build the project with GNU make: `$ make`.
* Optionally, install (requires `root`): `$ make install`.
* To uninstall use the following command (requires `root`): `$ make uninstall`.

The *runns* daemon will create a socket for communication with the clients.
Communication with this socket allowed only for users in the *runns* group.
* Create *runns* group: `groupadd runns`.
* Add *<USERNAME>* to the *runns* group: `usermod -a -G runns <USERNAME>`.

### runns
This is a main daemon. This daemon opens a UNIX socket, by default in
`/var/run/runns/runns.socket`, and provides logs via *syslog*.

### runnsctl
This is a client for the *runns* daemon. It allows to run a program inside the
specified network namespace.  It will copy all user shell environment
variables and program path to the daemon.
To add `argv` to the program enter them after a double hyphen, '--': `runnsctl --program foo -- --arg1 --arg2=bar`.

## Examples
### Run chromium
To run *chromium* inside a *foo* network namespace with a temporary profile:

`runnsctl --program /usr/bin/chromium --set-netns /var/run/netns/foo -- --temp-profile`

To stop the daemon:

`runnsctl --stop`

To list PIDs of the processes started by the user:

`runnsctl --list`

The other options could been seen with following command:

`runnsctl --help`

### Run a tmux session inside a network namespace

```shell
user$ # Run new tmux session with vpn123.socket
user$ runnsctl --create-ptms --program /usr/bin/tmux --set-netns /var/run/netns/vpn123 -- -L vpn123.socket
user$ # Attach tmux to the vpn123.socket
user$ tmux -2 -L vpn123.socket attach -d
```

Now you are inside the network namespace in the new tmux session.

## Troubleshooting

### Network managers

Some network managers have an issue with interfaces created by build-net and clean-net.
To resolve these issues please add the vpn interfaces (or according to yours naming convention)
to network manager's skip list.

For example in the case of [connman](https://01.org/connman) one could add
"vpn" to the **NetworkInterfaceBlacklist** in /etc/connman/main.conf.

```shell
$ grep ^NetworkInterfaceBlacklist /etc/connman/main.conf
NetworkInterfaceBlacklist = vmnet,vboxnet,virbr,ifb,ve-,vb-,vpn
```

## Acknowledgement

Thanks for the [nice font](https://fonts2u.com/amazdoomright.font) by Amazingmax which is used in the logo.
