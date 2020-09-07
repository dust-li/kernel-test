#!/bin/bash

if [ `whoami` != root ];then
	echo "run as root"
	exit 1
fi

saved_udp_mem=$(cat /proc/sys/net/ipv4/udp_mem)
saved_rmem_default=$(cat /proc/sys/net/core/rmem_default)
saved_rmem_max=$(cat /proc/sys/net/core/rmem_max)

new_udp_mem='4096  122846  184266'
new_rmem_default=$((2*4096*4096))
new_rmem_max=$((2*4096*4096))

# 1. setting udp_mem and rmem_xxx
echo $new_udp_mem > /proc/sys/net/ipv4/udp_mem
echo $new_rmem_default > /proc/sys/net/core/rmem_default
echo $new_rmem_max > /proc/sys/net/core/rmem_max

# Start the Udp server
python <<EOF &
#!/usr/bin/env python2

import socket
import time

ServerPort = 13344

address = ('0.0.0.0', ServerPort)
server_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
server_sock.bind(address)
print "server rcvbuf: %d" % server_sock.getsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF)

time.sleep(1000)
EOF
server_pid=$!
usleep 300000


# 2. start perf to watch sock:sock_exceed_buf_limit
perf trace -e 'sock:sock_exceed_buf_limit' &
perf_pid=$!
echo "Wait 2s for perf to start"
sleep 2


# 3. start client
python <<EOF
#!/usr/bin/env python2

import socket

ServerPort = 13344

address = ('127.0.0.1', ServerPort)
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

msg4096 = "".join(['0' for i in range(4096)])

# 3000 makes sure udp_prot.memory_allocated will exceed udp_mem[0]
# and sk_rmem_alloc won't exceed sk_rcvbuf
for _ in range(3000):
    sock.sendto(msg4096, address)
EOF


# 4. Cleanup & Recover
kill $server_pid
kill $perf_pid

echo $saved_udp_mem > /proc/sys/net/ipv4/udp_mem
echo $saved_rmem_default > /proc/sys/net/core/rmem_default
echo $saved_rmem_max > /proc/sys/net/core/rmem_max
