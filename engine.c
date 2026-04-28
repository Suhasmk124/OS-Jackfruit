#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>

#include "monitor_ioctl.h"

#define SOCKET_PATH "/tmp/engine.sock"
#define BUFFER_SIZE 10
#define MSG_SIZE 256

/* ================= BUFFER ================= */

char buffer[BUFFER_SIZE][MSG_SIZE];
int in = 0, out = 0, count_buf = 0;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;

void buffer_push(char *msg) {
    pthread_mutex_lock(&lock);

    while (count_buf == BUFFER_SIZE)
        pthread_cond_wait(&not_full, &lock);

    strcpy(buffer[in], msg);
    in = (in + 1) % BUFFER_SIZE;
    count_buf++;

    pthread_cond_signal(&not_empty);
    pthread_mutex_unlock(&lock);
}

char* buffer_pop() {
    static char msg[MSG_SIZE];

    pthread_mutex_lock(&lock);

    while (count_buf == 0)
        pthread_cond_wait(&not_empty, &lock);

    strcpy(msg, buffer[out]);
    out = (out + 1) % BUFFER_SIZE;
    count_buf--;

    pthread_cond_signal(&not_full);
    pthread_mutex_unlock(&lock);

    return msg;
}

/* ================= THREADS ================= */

void* producer(void *arg) {
    int fd = *(int*)arg;
    char buf[MSG_SIZE];

    while (1) {
        int n = read(fd, buf, sizeof(buf)-1);
        if (n <= 0) break;

        buf[n] = '\0';
        buffer_push(buf);
    }

    return NULL;
}

void* consumer(void *arg) {
    char *filename = (char*)arg;

    int fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);

    while (1) {
        char *msg = buffer_pop();
        write(fd, msg, strlen(msg));
    }

    close(fd);
    return NULL;
}

/* ================= CONTAINERS ================= */

typedef struct {
    char id[50];
    pid_t pid;
} Container;

Container containers[10];
int count = 0;

/* ================= COMMANDS ================= */

void start_container(char *id, char *rootfs, char *cmd, int client) {

    int pipefd[2];
    pipe(pipefd);

    pid_t pid = fork();

    if (pid == 0) {
        // CHILD

        close(pipefd[0]);

        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);

        close(pipefd[1]);

        if (chroot(rootfs) != 0) {
            perror("chroot failed");
            exit(1);
        }

        chdir("/");
        mount("proc", "/proc", "proc", 0, NULL);

        execl("/bin/sh", "sh", "-c", cmd, NULL);

        perror("exec failed");
        exit(1);
    }

    // PARENT

    close(pipefd[1]);

    containers[count].pid = pid;
    strcpy(containers[count].id, id);
    count++;

    char response[256];
    sprintf(response, "Started container %s PID %d\n", id, pid);
    write(client, response, strlen(response));

    // LOGGING THREADS
    pthread_t prod, cons;

    char logfile[100];
    sprintf(logfile, "logs/%s.log", id);

    mkdir("logs", 0755);

    pthread_create(&prod, NULL, producer, &pipefd[0]);
    pthread_create(&cons, NULL, consumer, logfile);

    // 🔥 IMPORTANT: wait so logs are captured
    waitpid(pid, NULL, 0);

    // KERNEL MONITOR
    int fd = open("/dev/container_monitor", O_RDWR);
    if (fd >= 0) {
        struct process_info info = {pid, 40, 64};
        ioctl(fd, REGISTER_PID, &info);
        close(fd);
    }
}

void list_containers(int client) {
    char response[1024] = "";

    if (count == 0) {
        strcpy(response, "No containers running\n");
    } else {
        for (int i = 0; i < count; i++) {
            char line[128];
            sprintf(line, "%s -> PID %d\n",
                    containers[i].id,
                    containers[i].pid);
            strcat(response, line);
        }
    }

    write(client, response, strlen(response));
}

void stop_container(char *id, int client) {
    char response[256] = "Container not found\n";

    for (int i = 0; i < count; i++) {
        if (strcmp(containers[i].id, id) == 0) {
            kill(containers[i].pid, SIGTERM);
            sprintf(response, "Stopped %s\n", id);
        }
    }

    write(client, response, strlen(response));
}

/* ================= SUPERVISOR ================= */

void run_supervisor() {
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);

    unlink(SOCKET_PATH);

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 5);

    printf("Supervisor running...\n");

    while (1) {
        int client = accept(server_fd, NULL, NULL);

        char buf[256] = {0};
        read(client, buf, sizeof(buf));

        char *cmd = strtok(buf, " ");

        if (strcmp(cmd, "start") == 0) {
            char *id = strtok(NULL, " ");
            char *rootfs = strtok(NULL, " ");
            char *exec = strtok(NULL, "\n");

            start_container(id, rootfs, exec, client);

        } else if (strcmp(cmd, "ps") == 0) {
            list_containers(client);

        } else if (strcmp(cmd, "stop") == 0) {
            char *id = strtok(NULL, " ");
            stop_container(id, client);

        } else if (strcmp(cmd, "exit") == 0) {
            char *msg = "Shutting down supervisor\n";
            write(client, msg, strlen(msg));
            close(client);
            exit(0);

        } else {
            char *msg = "Unknown command\n";
            write(client, msg, strlen(msg));
        }

        close(client);
    }
}

/* ================= CLI ================= */

void run_cli(int argc, char *argv[]) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("Error: Supervisor not running\n");
        return;
    }

    char buf[256] = {0};

    for (int i = 1; i < argc; i++) {
        strcat(buf, argv[i]);
        strcat(buf, " ");
    }

    write(sock, buf, strlen(buf));

    char response[1024] = {0};
    read(sock, response, sizeof(response));

    printf("%s", response);

    close(sock);
}

/* ================= MAIN ================= */

int main(int argc, char *argv[]) {
    if (argc < 2) return 1;

    if (strcmp(argv[1], "supervisor") == 0) {
        run_supervisor();
    } else {
        run_cli(argc, argv);
    }

    return 0;
}
