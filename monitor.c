#include <linux/mm.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/sched/signal.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#include "monitor_ioctl.h"

MODULE_LICENSE("GPL");

struct process_node {
    int pid;
    int soft;
    int hard;
    struct list_head list;
};

static LIST_HEAD(process_list);
static struct task_struct *monitor_thread;

long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    struct process_info info;
    struct process_node *node;

    if (copy_from_user(&info, (void *)arg, sizeof(info)))
        return -EFAULT;

    node = kmalloc(sizeof(*node), GFP_KERNEL);
    node->pid = info.pid;
    node->soft = info.soft_limit;
    node->hard = info.hard_limit;

    INIT_LIST_HEAD(&node->list);
    list_add(&node->list, &process_list);

    printk("Registered PID %d\n", node->pid);
    return 0;
}

static int monitor_fn(void *data) {
    while (!kthread_should_stop()) {
        struct process_node *node, *tmp;

        list_for_each_entry_safe(node, tmp, &process_list, list) {
            struct task_struct *task = pid_task(find_vpid(node->pid), PIDTYPE_PID);
            if (!task) continue;

            long rss = 0;
if (task->mm)
    rss = (get_mm_rss(task->mm)) * PAGE_SIZE;

            if (rss > node->soft * 1024 * 1024)
                printk("Soft limit exceeded PID %d\n", node->pid);

            if (rss > node->hard * 1024 * 1024) {
                printk("Hard limit exceeded. Killing PID %d\n", node->pid);
                send_sig(SIGKILL, task, 0);
            }
        }

        msleep(1000);
    }
    return 0;
}

static struct file_operations fops = {
    .unlocked_ioctl = device_ioctl,
};

static int major;

static int __init monitor_init(void) {
    major = register_chrdev(0, "container_monitor", &fops);
    monitor_thread = kthread_run(monitor_fn, NULL, "monitor_thread");
    printk("Monitor module loaded\n");
    return 0;
}

static void __exit monitor_exit(void) {
    kthread_stop(monitor_thread);
    unregister_chrdev(major, "container_monitor");
    printk("Monitor module removed\n");
}

module_init(monitor_init);
module_exit(monitor_exit);
