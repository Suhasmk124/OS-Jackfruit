# Mini Container Runtime (Operating Systems Project)

##  Overview

This project implements a lightweight container runtime similar to Docker using Linux system calls and kernel programming.

It demonstrates core Operating Systems concepts such as process isolation, inter-process communication, synchronization, and kernel-level monitoring.

---

##  Features

### 1️ Container Runtime

* Uses `fork()` and `exec()` to create processes
* Uses `chroot()` to isolate filesystem
* Runs commands inside a separate root filesystem

---

### 2️ Supervisor & CLI (IPC)

* Supervisor process manages all containers
* Communication via Unix Domain Sockets
* Supported commands:

  * `start`
  * `ps`
  * `stop`
  * `exit`

---

### 3️ Multi-Container Support

* Run multiple containers simultaneously
* Tracks:

  * Container ID
  * Process ID (PID)
  * Memory limits

---

### 4️ Logging System

* Captures container output using `pipe()`
* Implements Producer-Consumer problem:

  * Producer → reads from pipe
  * Consumer → writes to log file
* Uses:

  * Threads (`pthread`)
  * Mutex + Condition Variables
* Logs stored in:

  ```
  logs/<container_id>.log
  ```

---

### 5️ Root Filesystem (rootfs)

* Uses Alpine Linux minimal filesystem
* Provides required binaries (`/bin/sh`, `/bin/ls`, etc.)
* Each container runs inside its own rootfs

---

### 6️ Kernel Module (Memory Monitoring)

* Custom kernel module (`monitor.ko`)
* Registers container PIDs using `ioctl`
* Enforces memory limits:

  * Soft limit → warning
  * Hard limit → process termination

---

##  Setup Instructions

### Install Dependencies

```bash
sudo apt update
sudo apt install build-essential linux-headers-$(uname -r)
```

---

### Build Project

```bash
make
```

---

### Load Kernel Module

```bash
sudo insmod monitor.ko
```

---

### Run Supervisor

```bash
sudo ./engine supervisor
```

---

### Start Container

```bash
sudo ./engine start alpha ./rootfs-alpha "/bin/echo hello"
```

---

### View Running Containers

```bash
sudo ./engine ps
```

---

### View Logs

```bash
cat logs/alpha.log
```

---

### Stop Container

```bash
sudo ./engine stop alpha
```

---

### Stop Supervisor

```bash
sudo ./engine exit
```

---

##  Root Filesystem Setup

The root filesystem is **not included** in this repository due to size constraints.

To create it:

```bash
mkdir rootfs-base
cd rootfs-base

wget https://dl-cdn.alpinelinux.org/alpine/v3.20/releases/x86_64/alpine-minirootfs-3.20.3-x86_64.tar.gz
tar -xzf alpine-minirootfs-3.20.3-x86_64.tar.gz

cd ..
cp -a rootfs-base rootfs-alpha
mkdir -p rootfs-alpha/proc
```

---

##  Key Concepts Used

* Process Management (`fork`, `exec`)
* Filesystem Isolation (`chroot`)
* Inter-Process Communication (Unix Sockets, Pipes)
* Synchronization (Mutex, Condition Variables)
* Producer-Consumer Problem
* Kernel Programming (Modules, ioctl)

---

##  Example Demo

```bash
sudo ./engine start c1 ./rootfs-alpha "/bin/echo hello"
sudo ./engine start c2 ./rootfs-alpha "yes"

sudo ./engine ps

cat logs/c1.log
cat logs/c2.log
```

---

##  Authors

Suhas Mohan Kumar
T B Akash
