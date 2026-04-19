#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <time.h>

#include "monitor_ioctl.h"

#define MAX_CONTAINERS 10

typedef struct {
    char id[32];
    pid_t pid;
    unsigned long soft;
    unsigned long hard;
} container;

container containers[MAX_CONTAINERS];
int container_count = 0;

int monitor_fd = -1;

void register_container(const char *id, pid_t pid, unsigned long soft, unsigned long hard) {
    struct monitor_request req;
    memset(&req, 0, sizeof(req));

    req.pid = pid;
    req.soft_limit_bytes = soft;
    req.hard_limit_bytes = hard;
    strncpy(req.container_id, id, sizeof(req.container_id)-1);

    ioctl(monitor_fd, MONITOR_REGISTER, &req);
}

void unregister_container(const char *id, pid_t pid) {
    struct monitor_request req;
    memset(&req, 0, sizeof(req));

    req.pid = pid;
    strncpy(req.container_id, id, sizeof(req.container_id)-1);

    ioctl(monitor_fd, MONITOR_UNREGISTER, &req);
}

void save_metadata() {
    FILE *f = fopen("containers.db", "w");
    for(int i=0;i<container_count;i++){
        fprintf(f,"%s %d\n", containers[i].id, containers[i].pid);
    }
    fclose(f);
}

void load_metadata() {
    FILE *f = fopen("containers.db", "r");
    if(!f) return;

    container_count = 0;
    while(fscanf(f,"%s %d", containers[container_count].id, &containers[container_count].pid)==2){
        container_count++;
    }
    fclose(f);
}

void supervisor() {
    printf("Supervisor started\n");

    monitor_fd = open("/dev/container_monitor", O_RDWR);
    if (monitor_fd < 0) {
        perror("monitor device");
        exit(1);
    }

    while(1){
        sleep(1);
    }
}

void start_container(char *id, char *cmd, unsigned long soft, unsigned long hard) {
    pid_t pid = fork();

    if(pid == 0){
        execlp(cmd, cmd, NULL);
        perror("exec");
        exit(1);
    }

    containers[container_count].pid = pid;
    strncpy(containers[container_count].id, id, 31);
    containers[container_count].soft = soft;
    containers[container_count].hard = hard;

    container_count++;

    register_container(id, pid, soft, hard);

    save_metadata();

    printf("Started %s with PID %d\n", id, pid);
}

void list_containers() {
    load_metadata();

    for(int i=0;i<container_count;i++){
        printf("%s -> PID %d\n", containers[i].id, containers[i].pid);
    }
}

void stop_container(char *id) {
    load_metadata();

    for(int i=0;i<container_count;i++){
        if(strcmp(containers[i].id,id)==0){
            kill(containers[i].pid, SIGKILL);
            unregister_container(id, containers[i].pid);
            printf("Stopped %s\n", id);
        }
    }
}

void show_logs() {
    system("sudo dmesg | tail");
}

int main(int argc, char *argv[]) {

    if(argc < 2){
        printf("Usage\n");
        return 1;
    }

    if(strcmp(argv[1], "supervisor")==0){
        supervisor();
    }

    else if(strcmp(argv[1], "start")==0){
        if(argc < 4){
            printf("start <id> <command>\n");
            return 1;
        }

        monitor_fd = open("/dev/container_monitor", O_RDWR);

        start_container(argv[2], argv[3],
            50UL<<20, 80UL<<20);
    }

    else if(strcmp(argv[1], "ps")==0){
        list_containers();
    }

    else if(strcmp(argv[1], "logs")==0){
        show_logs();
    }

    else if(strcmp(argv[1], "stop")==0){
        if(argc < 3) return 1;
        stop_container(argv[2]);
    }

    return 0;
}
