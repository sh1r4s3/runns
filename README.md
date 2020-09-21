# ![GitHub Logo](/img/runns-logo.png)

## Contents  
* [About](#about)  
* [How to use it](#how-to-use-it)  
* [Troubleshooting](#troubleshooting)  
* [Example use-case](#example-use-case)  
* [Acknowledgement](#acknowledgement)  
* [References](#references)

## About
The **RUNNS** provides daemon (*runns*), client (*runnsctl*) and helper scripts (*build-net* and *clean-net*) for GNU/Linux to
easy create and delete network namespaces [1], connect this network namespace to default via veth pair [2] and setup iptables NAT rules.

**Why would you need this?** Let's assume a simple situation - you have a VPN with 0.0.0.0/1 default route (i.e. all of your traffic goes through VPN). However, VPN bandwidth is low and you don't want to pass some connections through VPN, for example SSH to another machine, or you simply don't want that all of your traffic goes through VPN. With runns it is easy to isolate programs inside a Linux network namespace with a VPN.

## How to use it
### Building
To build and run this project one need to have at least the following things:
* GNU make [3]
* GCC [4]
* libc (glibc) [5]
* binutils [6]
* POSIX compatible shell
* iptables [7]
* iproute2 [8]
* gawk [9]
* autoconf [10]

The building procedure is straight forward:

* Run autoconf in the source directory to get configure script: `$ autoconf`.
* Conifgure: `$ ./configure`.
* Build program with GNU make: `$ make`.
* Install: `$ make install`.
* To uninstall use the following command: `$ make uninstall`.

The **runns** daemon will create a socket for communication with clients.
Communication with this socket allowed only for users in the *runns* group.
* Create *runns* group: `groupadd runns`.
* Add a user USERNAME to the *runns* group: `usermod -a -G runns USERNAME`.
### runns
This is a main daemon. This daemon opens an UNIX socket, by default in `/var/run/runns/runns.socket`, and provides logs via *syslog*.
### runnsctl
This is a client for **runns** daemon. It allows to run a program inside the specified network namespace.
It will copy all user shell environment variables and program path to the daemon.
To add argv to the program enter them after the '--': `runnsctl --program foo -- --arg1 --arg2=bar`.

For example, to run a *chromium* inside the *foo* network namespace with temporary profile one could run:

`runnsctl --program /usr/bin/chromium --set-netns /var/run/netns/foo -- --temp-profile`

To stop the daemon:

`runnsctl --stop`

To list PIDs runned by the user:

`runnsctl --list`

The other options could been seen with following command:

`runnsctl --help`

### build-net
This helper script allow user to easy create a network namespace.
If run without arguments it will create a network namespace with a name vpn**X**, where **X**
is the maximum number + 1 of namespaces with names `vpn[0-9]+`. Also, it will create a veth pair and assign one
to the vpn**X** (it will be called eth0) and another to the default network namespace
(it will be called the same as namespace -- vpn**X**). The script will assign an IPv4 address to the veth pair:
* vpn**X** in the default namespace -- 172.24.**X**.1/24
* eth0 in the vpn**X** namespace -- 172.24.**X**.2/24

One could explicitly set the name of the veth interface in the default namespace and network namespace with the
`--int` and `--name` options.

Another useful feature is the `--resolve` option.
With this option it is possible to set different resolv.conf files for each network namespace.

At the end the script will also create an iptables NAT rule and setup resolv.conf if
it was mentioned in the command line arguments. The script will also automatically enable IPv4 forwarding.

This script also contains --section option. This option allows to load a specific section from the /etc/runns.conf
file. This file a plain case insensitive INI file and could contains the following options:
**NetworkNamespace**, **InterfaceIn**, **InterfaceOut**, **Resolve**.
For example:
```shell
$ cat /etc/runns.conf
[work]
NetworkNamespace=vpnWork
InterfaceIn=vpnWorkd
vpn=/etc/openvpn/work.conf
vpn=/etc/openvpn/myvps.conf
```
Each vpn option specifies configuration files for OpenVPN [13]. OpenVPN will be started as a daemon
with a "openvpn-**NetworkNamespaceName**" name in the system logger.

### clean-net
This helper script is needed to easy delete and clean network namespace created by **build-net**.
This script will check if any program is running inside the network namespace and if so it will ask to try
to kill them all automatically.
Please check the options before use: `clean-net --help`.

## Example use-case

Let's assume that there is a /etc/runns.conf configuration file from above.  
From **root** user:

```shell
root$ build-net -s work
root$ runns
```

From **iddqd** user:
```shell
iddqd$ runnsctl --program /usr/bin/chromium --set-netns /var/run/netns/vpnWork
```

To clean-up:
```shell
root$ clean-net --name vpnWork -f
root$ runnsctl -s
```

### Run tmux session inside network namespace

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

For example in the case of connman [12] one could add "vpn" to the **NetworkInterfaceBlacklist**
in /etc/connman/main.conf.

```shell
$ grep ^NetworkInterfaceBlacklist /etc/connman/main.conf
NetworkInterfaceBlacklist = vmnet,vboxnet,virbr,ifb,ve-,vb-,vpn
```

## Acknowledgement

Thanks for the nice font [11] by Amazingmax which is used in the logo.

## References
1 -- https://lwn.net/Articles/580893

2 -- https://lwn.net/Articles/237087

3 -- https://www.gnu.org/software/make

4 -- https://gcc.gnu.org

5 -- https://www.gnu.org/software/libc

6 -- https://www.gnu.org/software/binutils

7 -- http://www.netfilter.org/projects/iptables

8 -- https://wiki.linuxfoundation.org/networking/iproute2

9 -- https://www.gnu.org/software/gawk

10 -- https://www.gnu.org/software/autoconf

11 -- https://fonts2u.com/amazdoomright.font

12 -- https://01.org/connman

13 -- https://openvpn.net
