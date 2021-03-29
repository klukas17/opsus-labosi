#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>

int result = 0;

void* dretva(void* thread_argument) {
    int count = *((int*)thread_argument);
    for (int i = 0; i < count; i++) result++;
}

int main(int argc, char *argv[]){
    int threadCount = atoi(argv[1]);
    int iterationCount = atoi(argv[2]);
    pthread_t thread_id[threadCount];
    int argumenti[threadCount];

    for (int i = 0; i < threadCount; i++) {
        argumenti[i] = iterationCount;
        if (pthread_create(&thread_id[i], NULL, dretva, &argumenti[i])) {
            printf("Nije bilo moguće stvoriti dretvu.\n");
            exit(1);
        }
    }

    for (int i = 0; i < threadCount; i++) pthread_join(thread_id[i], NULL);

    printf("A = %d\n", result);
}