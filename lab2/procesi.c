#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <signal.h>

int processCount, iterationCount, *result = 0, sharedMemoryId;

int proces() {
    for (int i = 0; i < iterationCount; i++) (*result)++;
}

void prekid() {
    shmdt((char*)sharedMemoryId);
    shmctl(sharedMemoryId, IPC_RMID, NULL);
}

int main(int argc, char *argv[]) {
    processCount = atoi(argv[1]);
    iterationCount = atoi(argv[2]);
    sharedMemoryId = shmget(IPC_PRIVATE, sizeof(int), 0600);

    if (sharedMemoryId == -1) {
        printf("Greska. Nije bilo moguce stvoriti zajednicku memoriju.\n");
        exit(1);
    }

    result = (int*) shmat(sharedMemoryId, NULL, 0);

    sigset(SIGINT, prekid);

    for (int i = 0; i < processCount; i++) {
        switch(fork()) {
            case -1:
                printf("Nije bilo moguce stvoriti novi proces.\n");
            case 0:
                proces();
                exit(0);
            default:
                continue;
        }
    }
    
    for (int i = 0; i < processCount; i++) wait(NULL);

    printf("A = %d\n", *result);

    prekid();
}