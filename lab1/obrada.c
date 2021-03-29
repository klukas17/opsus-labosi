#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdlib.h>
 
#define N 6    /* broj razina proriteta */
 
int OZNAKA_CEKANJA[N];
int PRIORITET[N];
int TEKUCI_PRIORITET;
int sig[]={SIGUSR1, SIGUSR2, SIGTERM, SIGQUIT, SIGINT};

void zabrani_prekidanje() {
    for(int i = 0; i < 5; i++) sighold(sig[i]);
}

void dozvoli_prekidanje() {
    for(int i = 0; i < 5; i++) sigrelse(sig[i]);
}
 
void obrada_prekida(int i){
    switch(i) {
        case 1:
            printf("- P - - - -\n");
            for (int j = 1; j <= 5; j++) {
                sleep(1);
                printf("- %d - - - -\n", j);
            }
            printf("- K - - - -\n");
            break;
        case 2:
            printf("- - P - - -\n");
            for (int j = 1; j <= 5; j++) {
                sleep(1);
                printf("- - %d - - -\n", j);
            }
            printf("- - K - - -\n");
            break;
        case 3:
            printf("- - - P - -\n");
            for (int j = 1; j <= 5; j++) {
                sleep(1);
                printf("- - - %d - -\n", j);
            }
            printf("- - - K - -\n");
            break;
        case 4:
            printf("- - - - P -\n");
            for (int j = 1; j <= 5; j++) {
                sleep(1);
                printf("- - - - %d -\n", j);
            }
            printf("- - - - K -\n");
            break;
        case 5:
            printf("- - - - - P\n");
            for (int j = 1; j <= 5; j++) {
                sleep(1);
                printf("- - - - - %d\n", j);
            }
            printf("- - - - - K\n");
            break;
    }
}

void prekidna_rutina(int sig){
   int n = -1;
   zabrani_prekidanje();
    switch(sig){
        case SIGUSR1: 
            n = 1; 
            printf("- X - - - -\n");
            break;
        case SIGUSR2: 
            n = 2; 
            printf("- - X - - -\n");
            break;
        case SIGTERM:
            n = 3;
            printf("- - - X - -\n");
            break;
        case SIGQUIT:
            n = 4;
            printf("- - - - X -\n");
            break;
        case SIGINT:
            n = 5;
            printf("- - - - - X\n");
            break;
   }
   OZNAKA_CEKANJA[n]++;
   int x;
   do {

       x = 0;
       for (int j = TEKUCI_PRIORITET + 1; j < N; j++) {
           if (OZNAKA_CEKANJA[j] != 0) {
               x = j;
           }
       }
       
       if (x > 0) {
           OZNAKA_CEKANJA[x]--;
           PRIORITET[x] = TEKUCI_PRIORITET;
           TEKUCI_PRIORITET = x;
           dozvoli_prekidanje();
           obrada_prekida(x);
           zabrani_prekidanje();
           TEKUCI_PRIORITET = PRIORITET[x];
       }

   } while (x > 0);
   dozvoli_prekidanje();
}
 
int main(void) {
    sigset(SIGUSR1, prekidna_rutina);
    sigset(SIGUSR2, prekidna_rutina);
    sigset(SIGTERM, prekidna_rutina);
    sigset(SIGQUIT, prekidna_rutina);
    sigset(SIGINT, prekidna_rutina);
    
    printf("Proces obrade prekida, PID=%d\n", getpid());
    
    int i = 0;
    while(1) {
        sleep(1);
        i++;
        i %= 10;
        printf("%d - - - - -\n", i);
    }
    
    printf ("Zavrsio osnovni program\n");
    
    return 0;
}

/*
    detaljniji način ispisa stanja programa:
    printf("%d - - - - -   O_CEK[%d %d %d %d %d %d] TEK_PRIOR = %d PRIOR[%d %d %d %d %d %d]\n", 
        i, OZNAKA_CEKANJA[0], OZNAKA_CEKANJA[1], OZNAKA_CEKANJA[2], OZNAKA_CEKANJA[3],
        OZNAKA_CEKANJA[4], OZNAKA_CEKANJA[5], TEKUCI_PRIORITET, PRIORITET[0],
        PRIORITET[1], PRIORITET[2], PRIORITET[3], PRIORITET[4], PRIORITET[5]);
*/