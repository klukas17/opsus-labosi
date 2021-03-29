#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <math.h>
#include <stdlib.h>

int pauza = 0;
unsigned long broj = 1000000001;
unsigned long zadnji = 1000000001;

void periodicki_ispis() {
    printf("%lu\n", zadnji);
}

void postavi_pauzu() {
    pauza = 1 - pauza;
}

void prekini() {
    printf("%lu\n", zadnji);
    exit(0);
}

int prost(unsigned long broj) {
    unsigned long i, max;
    if (broj % 2 == 0) return 0;
    max = sqrt(broj);
    for (i = 3; i <= max; i++) if (broj % i == 0) return 0;
    return 1;
}

int main(void) {

    struct itimerval t;

    sigset(SIGALRM, periodicki_ispis);
    sigset(SIGINT, postavi_pauzu);
    sigset(SIGTERM, prekini);

    t.it_value.tv_sec = 5;
    t.it_value.tv_usec = 0;
    t.it_interval.tv_sec = 5;
    t.it_interval.tv_usec = 0;

    setitimer(ITIMER_REAL, &t, NULL);

    while(1) {
    	if(prost(broj)) zadnji = broj;
    	broj++;
    	while (pauza == 1) continue;
    }
}
