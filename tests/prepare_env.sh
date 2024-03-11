#!/bin/sh -ex

TEST_NETNS=test_netns
VETH_DEF=test_veth_default
VETH_NETNS=test_veth_netns

fatal() {
    printf "fatal error: %s\n" "${1-}"
    exit 1
}

clean() {
    ip netns exec "$TEST_NETNS" ip link del "$VETH_NETNS" || :
    ip link del "$VETH_DEF" || :
    ip netns del "$TEST_NETNS" || :
}

trap clean EXIT HUP PIPE INT QUIT TERM

# Check user
[ "$UID" = "0" ] || fatal "User $USER is not root"

# Create NETNS and test veth interfaces
ip netns add "$TEST_NETNS"
ip link add "$VETH_DEF" type veth peer name "$VETH_NETNS"
ip link set "$VETH_NETNS" netns "$TEST_NETNS"

# Set IPs
ip addr add "172.24.0.1/24" dev "$VETH_DEF"
ip netns exec "TEST_NETNS" ip addr add "172.24.1.2/24" dev "$VETH_NETNS"
ip netns exec "TEST_NETNS" ip link set "$VETH_NETNS" up
ip link set "$VETH_DEF" up

# Enable IPv4 forwarding
echo "1" > /proc/sys/net/ipv4/ip_forward
