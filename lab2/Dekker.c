#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <stdatomic.h>
#include <signal.h>

int processCount, iterationCount, sharedMemoryId;
atomic_int *result = 0, *turn = 0, *flag;

void prekid() {
    shmdt((char*)sharedMemoryId);
    shmctl(sharedMemoryId, IPC_RMID, NULL);
}

int proces(int processNumber) {
    for (int i = 0; i < iterationCount; i++) {
        flag[processNumber] = 1;
        while (flag[1 - processNumber] != 0) {
            if (*turn != processNumber) {
                flag[processNumber] = 0;
                while (*turn != processNumber);
                flag[processNumber] = 1;
            }
        }
        (*result)++;
        *turn = 1 - processNumber;
        flag[processNumber] = 0;
    }
}

int main(int argc, char *argv[]) {
    processCount = 2;
    iterationCount = atoi(argv[1]);
    sharedMemoryId = shmget(IPC_PRIVATE, 4 * sizeof(int), 0600);

    if (sharedMemoryId == -1) {
        printf("Greska! Nije bilo moguce stvoriti zajednicku memoriju.\n");
        exit(1);
    }

    result = (atomic_int*) shmat(sharedMemoryId, NULL, 0);
    turn = result + 1;
    flag = result + 2;

    sigset(SIGINT, prekid);

    for (int i = 0; i < processCount; i++) {
        switch(fork()) {
            case -1:
                printf("Nije bilo moguce stvoriti novi proces.\n");
            case 0:
                proces(i);
                exit(0);
            default:
                continue;
        }
    }
    for (int i = 0; i < processCount; i++) wait(NULL);

    printf("A = %d\n", *result);
    prekid();
}