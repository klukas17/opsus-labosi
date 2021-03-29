#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

int pid=0;

void prekidna_rutina(int sig){
   kill(pid, SIGKILL);
   exit(0);
}

int main(int argc, char *argv[]){
   pid = atoi(argv[1]);
   sigset(SIGINT, prekidna_rutina);
   srand(time(NULL));

    while(1){
        int r = (rand() % 3) + 3;
        sleep(r);
        int s = (rand() % 4) + 1;
        switch(s) {
            case 1:
                printf("Saljem prekid 1. razine!\n");
                kill(pid, SIGUSR1);
                break;
            case 2:
                printf("Saljem prekid 2. razine!\n");
                kill(pid, SIGUSR2);
                break;
            case 3:
                kill(pid, SIGTERM);
                printf("Saljem prekid 3. razine!\n");
                break;
            case 4:
                kill(pid, SIGQUIT);
                printf("Saljem prekid 4. razine!\n");
                break;
        }
   }
   return 0;
}