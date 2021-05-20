#define _XOPEN_SOURCE 500

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

// u zadatku je 5 filozofa
#define FILOZOFI 5

// struktura za čuvanje svih podataka potrebnih za sinkronizaciju
struct {
    pthread_mutex_t monitor;
    pthread_cond_t uvjeti[FILOZOFI];
    int stapici[FILOZOFI];
    char stanjeFilozofa[FILOZOFI];
} v;

// funkcija kojom dretva ulazi u monitor
void pocetak_hranjenja(int filozofIndeks) {

    // postavlja dretvu u red čekanja za monitor
    pthread_mutex_lock(&(v.monitor));

    // uvjet blokiranja dretve unutar monitora
    while (v.stapici[filozofIndeks] == 0 || v.stapici[(filozofIndeks+1)%FILOZOFI] == 0)
        pthread_cond_wait(&(v.uvjeti[filozofIndeks]), &(v.monitor));

    // zauzimanje resursa (štapića)
    v.stapici[filozofIndeks] = 0;
    v.stapici[(filozofIndeks + 1) % FILOZOFI] = 0;

    // promjena stanja filozofa - filozof sad kreće jesti
    v.stanjeFilozofa[filozofIndeks] = 'X';

    // izlazak iz monitora
    pthread_mutex_unlock(&(v.monitor));
}

// funkcija kojom dretva izlazi iz monitora
void kraj_hranjenja(int filozofIndeks) {

    // postavlja dretvu u red čekanja za monitor
    pthread_mutex_lock(&(v.monitor));

    // oslobađanje resursa (štapića)
    v.stapici[filozofIndeks] = 1;
    v.stapici[(filozofIndeks + 1) % FILOZOFI] = 1;

    // oslobađanje (potencijalno) blokiranih dretvi čiji filozofi su za stolom slijeva i zdesna trenutnog filozofa
    pthread_cond_signal(&(v.uvjeti[(filozofIndeks + 4) % FILOZOFI]));
    pthread_cond_signal(&(v.uvjeti[(filozofIndeks + 1) % FILOZOFI]));

    // promjena stanja filozofa - filozof sad misli
    v.stanjeFilozofa[filozofIndeks] = 'O';

    // izlazak iz monitora
    pthread_mutex_unlock(&(v.monitor));
}

// dretva filozof (upravo dretve filozof sinkroniziramo monitorom)
void* filozof(void* thread_argument) {

    // dretva spava jednu sekundu kako bi ispis kraja inicijalizacije programa bio prvi
    sleep(1);

    // dereferenciranje argumenta dretve koji označava njen indeks za stolom
    int filozofIndeks = *((int*)thread_argument);

    // kontrolni ispis stvaranja filozofa
    printf("Stvoren je filozof %d\n", filozofIndeks + 1);

    // dretva spava jednu sekundu kako se ispisi za stolom ne bi miješali s kontrolnim ispisma stvaranja dretvi
    sleep(1);

    // ciklička petlja koju svaka dretva filozof izvodi
    while (1) {

        // filozof spava tj. misli određen broj sekundi (između 1 i 5)
        sleep(1 + rand() % 5);

        // promjena stanja filozofa - filozof sad želi jesti i čeka štapiće
        v.stanjeFilozofa[filozofIndeks] = 'o';

        // poziv funkcije kojom će dretva ući u monitor s namjerom da krene jesti
        pocetak_hranjenja(filozofIndeks);

        // kontrolni ispis trenutnog stanja za stolom
        printf("%c %c %c %c %c (%d)\n", v.stanjeFilozofa[0], v.stanjeFilozofa[1],
            v.stanjeFilozofa[2], v.stanjeFilozofa[3], v.stanjeFilozofa[4],
            filozofIndeks+1);

        // filozof spava tj. jede određen broj sekundi (između 1 i 5)
        sleep(1 + rand() % 5);

        // poziv funkcije kojom će dretva ući u monitor s namjerom da oslobodi druge dretve i omogući im da jedu
        kraj_hranjenja(filozofIndeks);
    }

}

// funkcija koja oslobađa sve resurse pri slanju signala SIGINT
void krajPrograma() {

    // uništi monitor
    pthread_mutex_destroy(&(v.monitor));

    // uništi redove čekanja na neki uvjet
    for (int i = 0; i < FILOZOFI; i++) pthread_cond_destroy(&(v.uvjeti[i]));

    // izađi iz programa
    exit(0);
}

// glavni program
int main() {

    // stvaranje monitora
    pthread_mutex_init(&(v.monitor), NULL);

    // stvaranje redova uvjeta, te inicijalno postavljanje varijabli štapića i stanja za svakog filozofa
    for (int i = 0; i < FILOZOFI; i++) {
        pthread_cond_init(&(v.uvjeti[i]), NULL);
        v.stapici[i] = 1;
        v.stanjeFilozofa[i] = 'O';
    }

    // polja argumenata potrebna za stvaranje dretvi
    pthread_t thread_id[FILOZOFI];
    int argumenti[FILOZOFI];

    // kada pošaljemo SIGINT, u ovoj funkciji se oslobađaju svi resursi i završava program
    sigset(SIGINT, krajPrograma);

    // stvaranje dretvi filozofa
    for (int i = 0; i < FILOZOFI; i++) {
        argumenti[i] = i;
        if (pthread_create(&thread_id[i], NULL, filozof, &argumenti[i])) {
            printf("Nije bilo moguće stvoriti dretvu nekog filozofa!\n");
            exit(1);
        }
    }

    // postavljanje seed-a za generiranje nasumičnih brojeva
    srand(time(NULL));

    // kontrolni ispis kraja postavljanja okoline za izvođenje programa
    printf("Inicijalizacija je završena!\n");

    // čekanje dretvi koje smo stvorili da završe, s tim da su one cikličke, pa ovime onemogućujemo da program prerano stane
    for (int i = 0; i < FILOZOFI; i++) pthread_join(thread_id[i], NULL);
}