## Overview
This repo contains some kernel test programs

#### exceed_udp_mem_min_drop
A test program reproduce the kernel bug when udp_prot.memory_allocated exceed
net.ipv4.udp_mem[0], udp packets got dropped.

Just run `bash exceed_udp_mem_min_drop.sh` to reproduce the problem

#### simu_hang_task
A kernel module that simulate kernel thread hang or timer hang.
It's useful when debugging some kernel bugs which need to trigger
when some CPU hang for some time.

I use this module to reproduce the TCP loopback retransmittion
problem ['tcp: avoid spurious loopback retransmit']
