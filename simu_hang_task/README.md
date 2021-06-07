# WHAT is this module ?
This module is a kernel module to simulate task hang
or timer hang.
It's a debug kernel module for replaying some cases which
triggered by a hang in kernel.

This module is used for debug only, and **MUST NOT** be
used on a online server.

# HOW to use
This module using sysctl to trigger the hang task or timer.

Usage:

### 1. List available sysctl arguments:
```bash
 #sysctl  -a | grep simu_hang_task
 simu_hang_task.available_type = thread timer
 simu_hang_task.current_type = thread
 simu_hang_task.delay_us = 5000
```

### 2. Delay the current sysctl thread for 500ms:
```bash
    #sysctl -w simu_hang_task.delay_us=5000000
```

This will cause sysctl hang for 500ms on the current CPU.

### 3. Add a timer which will run 500ms for once:
```bash
 #sysctl -w simu_hang_task.current_type=timer
 #sysctl -w simu_hang_task.delay_us=5000000
```

Note, the timer will be run on the CPU where sysctl runs,
so if you want the timer to run on a specific CPU, use
`taskset -c $CPU sysctl -w simu_hang_task.delay_us=5000000`
