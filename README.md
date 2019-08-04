# ![GitHub Logo](/img/runns-logo.png)
## About
The **RUNNS** provides daemon (*runns*), client (*runnsctl*) and helper scripts (*build-net* and *clean-net*) for GNU/Linux to
easy create and delete network namespaces [1], connect this network namespace to default via veth pair [2] and setup iptables NAT rules.

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

To build the binaries just run `make` inside your project directory.
The **runns** daemon will create a socket for communication with clients.
Communication with this socket allowed only for users in the *runns* group.
* Create *runns* group: `groupadd runns`
* Add a user USERNAME to the *runns* group: `usermod -a -G runns USERNAME`
### runns
This is a main daemon. This daemon opens an UNIX socket in `/var/run/runns/runns.socket` and provides logs via *syslog*.
### runnsctl
This is a client for **runns** daemon. It allows to run a program inside the specified network namespace.
It will copy all user shell environment variables and program path to the daemon.
To add argv to the program enter them after the '--': `./runnsctl --program foo -- --arg1 --arg2=bar`.

For example, to run a *chromium* inside the *foo* network namespace with temporary profile one could run:

`./runnsctl --program /usr/bin/chromium --netns /var/run/netns/foo -- --temp-profile`

To stop the daemon:

`./runnsctl --stop`

To list PIDs runned by the user:

`./runnsctl --list`

The other options could been seen with following command:

`./runnsctl --help`

### build-net
This helper script allow user to easy create a network namespace.
If run without arguments it will create a network namespace with a name vpn**X**, where **X**
is the maximum number + 1 of namespaces with names `vpn[0-9]+`. Also, it will create a veth pair and assign one
to the vpn**X** (it will be called eth0) and another to the default network namespace
(it will be called the same as namespace -- vpn**X**). The script will assign an IPv4 address to the veth pair:
* vpn**X** in the default namespace -- 172.0.**X**.1/24
* eth0 in the vpn**X** namespace -- 172.0.**X**.2/24

One could explicitly set the name of the veth interface in the default namespace and network namespace with the
`--int` and `--name` options.

Another useful feature is the `--resolve` option.
With this option it is possible to set different resolv.conf files for each network namespace. 

At the end the script will also create an iptables NAT rule and setup resolv.conf if
it was mentioned in the command line arguments. The script will also automatically enable IPv4 forwarding.

### clean-net
This helper script is needed to easy delete and clean network namespace created by **build-net**.
This script will check if any program is running inside the network namespace and if so it will ask to try
to kill them all automatically.
Please check the options before use: `./clean-net --help`.

### Example use-case

From **root** user:

```shell
root$ ./build-net
root$ ip netns exec vpn1 openvpn /etc/openvpn/config
root$ ./runns
```

From **iddqd** user:
```shell
iddqd$ ./runnsctl --program /usr/bin/chromium --netns /var/run/netns/vpn1
...
iddqd$ ./runnsctl -s
```

To clean-up:
```shell
root$ ./clean-net --name vpn1 -f
```

## Acknowledgement

Thanks for the nice font [10] by Amazingmax which is used in the logo.

## Refs
1 -- https://lwn.net/Articles/580893

2 -- https://lwn.net/Articles/237087

3 -- https://www.gnu.org/software/make

4 -- https://gcc.gnu.org

5 -- https://www.gnu.org/software/libc

6 -- https://www.gnu.org/software/binutils

7 -- http://www.netfilter.org/projects/iptables

8 -- https://wiki.linuxfoundation.org/networking/iproute2

9 -- https://www.gnu.org/software/gawk

10 -- https://fonts2u.com/amazdoomright.font
