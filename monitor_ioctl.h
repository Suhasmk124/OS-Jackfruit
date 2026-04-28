#ifndef MONITOR_IOCTL_H
#define MONITOR_IOCTL_H

#define REGISTER_PID _IOW('a', 'a', int *)

struct process_info {
    int pid;
    int soft_limit;
    int hard_limit;
};

#endif
