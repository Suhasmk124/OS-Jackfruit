Mini Container Runtime (OS Project)
Overview

This project implements a lightweight container runtime similar to Docker using Linux system calls.

It supports:

    Process isolation using chroot

    Container lifecycle management via a supervisor

    Logging using producer-consumer model

    Kernel module for memory monitoring

Features
1. Container Runtime

    Uses fork() and exec() to run processes

    Uses chroot() for filesystem isolation

2. Supervisor (IPC)

    Unix domain sockets for communication

    Supports commands:

        start

        stop

        ps

        exit

3. Logging System

    Captures stdout/stderr using pipes

    Producer-consumer model with threads

    Logs stored in logs/<container>.log

4. Kernel Module

    Registers container PIDs using ioctl

    Monitors memory usage

    Soft limit → warning

    Hard limit → process kill

Setup Instructions
Install dependencies

sudo apt update
sudo apt install build-essential linux-headers-$(uname -r)

Build

make

Load kernel module

sudo insmod monitor.ko

Run supervisor

sudo ./engine supervisor

Run container

sudo ./engine start alpha ./rootfs-alpha "/bin/echo hello"

Demo

sudo ./engine start c1 ./rootfs-alpha "/bin/echo hello"
sudo ./engine ps
cat logs/c1.log

Concepts Used

    Process Management

    Inter Process Communication (IPC)

    Synchronization (Mutex, Condition Variables)

    Kernel Programming

    Filesystem Isolation
