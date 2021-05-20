#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

// struktura za čuvanje svih podataka potrebnih za sinkronizaciju
struct {
    int brSobova;
    int brPatuljaka;
    int brDjece;
    int djedRadiKonzultacije;
    sem_t djed;
    sem_t sobovi;
    sem_t patuljci;
    sem_t djeca;
    sem_t KO;
    sem_t glavni;
    pid_t polPid;
    pid_t djedPid;
} *v;

// dretva djed Božićnjak
void djedBozicnjak() {

    // kontrolni ispis stvaranja djeda Božićnjaka
    printf("Djed Božićnjak je stvoren!\n");

    while(1) {

        // čekaj semafor koji označava da se djed Božićnjak mora probuditi
        sem_wait(&(v->djed));

        // čekaj semafor za ulazak u K.O.
        sem_wait(&(v->KO));

        // djed treba raznositi poklone
        if (v->brSobova == 10 && v->brPatuljaka > 0) {

            // postavi semafor za ulazak u K.O.
            sem_post(&(v->KO));
            printf("Djed raznosi poklone djeci.\n");

            // daj poklon svakom djetetu koje čeka poklon
            while (v->brDjece > 0) {

                // simuliraj davanje poklona
                sleep(1);

                // čekaj semafor za ulazak u K.O.
                sem_wait(&(v->KO));

                // umanji broj djece za 1
                v->brDjece--;

                // postavi semafor za ulazak u K.O.
                sem_post(&(v->KO));

                // postavi semafor koji čekaju dretve djeca
                sem_post(&(v->djeca));
            }

            // djed je završio s raznošenjem poklona
            printf("Djed je završio s raznošenjem poklona.\n");

            // postavi semafor za ulazak u K.O.
            sem_post(&(v->KO));

            // postavi semafor koji čekaju dretve sobovi ukupno 10 puta
            for (int i = 0; i < 10; i++) sem_post(&(v->sobovi));

            // postavi broj sobova na 0
            v->brSobova = 0;
        }

        // djed treba nahraniti sobove
        if (v->brSobova == 10) {

            // postavi semafor za ulazak u K.O.
            sem_post(&(v->KO));

            // simuliraj hranjenje sobova
            printf("Djed hrani sobove.\n");
            sleep(2);

            // čekaj semafor za ulazak u K.O.
            sem_wait(&(v->KO));
        }

        // djed treba odraditi konzultacije s patuljcima
        while (v->brPatuljaka >= 3) {

            // označi da radiš konzultacije s patuljcima
            v->djedRadiKonzultacije = 1;

            // postavi semafor za ulazak u K.O.
            sem_post(&(v->KO));

            // simuliranje konzultacija s patuljcima
            printf("Djed radi konzultacije s patuljcima.\n");
            sleep(2);

            // čekaj semafor za ulazak u K.O.
            sem_wait(&(v->KO));

            // postavi semafor koji čekaju patuljci ukupno 3 puta
            for (int i = 0; i < 3; i++) sem_post(&(v->patuljci));

            // umanji broj patuljaka za 3
            v->brPatuljaka -= 3;

            // označi da više ne radiš konzultacije s patuljcima
            v->djedRadiKonzultacije = 0;
            printf("Djed je završio s konzultacijama. Sada je broj patuljaka koji čeka djeda jednak %d.\n", v->brPatuljaka);
        }

        // postavi semafor za ulazak u K.O.
        sem_post(&(v->KO));

    }

}

// proces patuljak
void patuljak() {
    
    // čekaj na semaforu za ulazak u K.O.
    sem_wait(&(v->KO));

    // stvoren je patuljak, i stoga treba povećati broj patuljaka
    v->brPatuljaka++;
    printf("Stvoren je novi patuljak - broj patuljaka koji čeka djeda je %d.\n", v->brPatuljaka);

    // ako je zadovoljen bilo koji uvjet za buđenje djeda božićnjaka, probudi ga
    if (v->brPatuljaka >= 3 || v->brSobova == 10) sem_post(&(v->djed));

    // postavi semafor za ulazak u K.O.
    sem_post(&(v->KO));

    // čekaj semafor koji označava da je djed završio s konzultacijama s patuljcima
    sem_wait(&(v->patuljci));
    printf("Proces patuljak je završio s radom.\n");
}

// proces sob
void sob() { 

    // čekaj na semaforu za ulazak u K.O.
    sem_wait(&(v->KO));

    // stvoren je sob, i stoga treba povećati broj sobova
    v->brSobova++;
    printf("Stvoren je novi sob - broj sobova koji sada čeka djeda je %d.\n", v->brSobova);

    // ako je zadovoljen uvjet za buđenje djeda božićnjaka, probudi ga
    if (v->brSobova == 10) sem_post(&(v->djed));

    // postavi semafor za ulazak u K.O.
    sem_post(&(v->KO));

    // čekaj semafor koji označava da je djed razvezao poklone
    sem_wait(&(v->sobovi));
    printf("Proces sob je završio s radom.\n");
}

// proces dijete
void dijete() {

    // čekaj na semaforu za ulazak u K.O.
    sem_wait(&(v->KO));

    // stvoreno je dijete, i stoga treba povećati broj djece
    v->brDjece++;
    printf("Pronađeno je novo dijete - broj djece koji sada čeka poklone je %d.\n", v->brDjece);

    // postavi semafor za ulazak u K.O.
    sem_post(&(v->KO));

    // čekaj na semaforu koji označava da je dijete primilo poklon
    sem_wait(&(v->djeca));
    printf("Dijete je primilo poklon! Proces dijete je završio s radom.\n");
}

// proces sjeverni pol
void sjeverniPol() {

    // kontrolni ispis stvaranja sjevernog pola
    printf("Sjeverni pol je stvoren!\n");

    // postavljanje seeda za generiranje slučajnih brojeva
    srand(time(NULL));

    // ciklički posao dretve sjeverni pol
    while(1) {

        // spavaj nasumičan broj sekundi (1-3)
        sleep(rand() % 3 + 1);

        // čekaj na semaforu za ulazak u K.O.
        sem_wait(&(v->KO));

        // stvaranje procesa soba
        if (v->brSobova < 10 && rand() % 10 > 5) {
            switch(fork()) {
                case 0:
                    sob();
                    exit(0);
            }
        }

        // stvaranje procesa patuljka
        if (rand() % 10 > 5 && !(v->djedRadiKonzultacije)) {
            switch(fork()) {
                case 0:
                    patuljak();
                    exit(0);
            }
        }

        // stvaranje procesa djeteta
        if (rand() % 10 > 7) {
            switch(fork()) {
                case 0:
                    dijete();
                    exit(0);
            } 
        }

        // postavljanje semafora za ulazak u K.O.
        sem_post(&(v->KO));
    }
}

// funkcija koja oslobađa sve resurse i gasi program prilikom slanja signala SIGINT
void krajPrograma() {

    // ubijanje procesa djeda božićnjaka i sjevernog pola
    kill(v->djedPid, SIGKILL);
    kill(v->polPid, SIGKILL);

    // uništavanje svih semafora
    sem_destroy(&(v->djed));
    sem_destroy(&(v->sobovi));
    sem_destroy(&(v->patuljci));
    sem_destroy(&(v->djeca));
    sem_destroy(&(v->KO));
    sem_destroy(&(v->glavni));

    // izlazak iz programa
    exit(0);
}

// glavni program
int main() {

    // dohvaćanje identifikatora memorijskog segmenta
    int shmID = shmget(IPC_PRIVATE, sizeof(v), 0600);

    // nije bilo moguće dohvatiti memorijski segment, program treba završiti
    if (shmID == -1) {
        printf("Greska. Nije bilo moguce stvoriti zajednicku memoriju.\n");
        exit(1);
    }

    // vezanje strukture v u kojoj se nalaze sve varijable na dohvaćeni memorijski segment
    v = shmat(shmID, NULL, 0);

    // početno postavljanje varijabli
    v->brPatuljaka = 0;
    v->brSobova = 0;
    v->brDjece = 0;
    v->djedRadiKonzultacije = 0;

    // postavljanje željenog ponašanja u slučaju pojave signala SIGINT
    sigset(SIGINT, krajPrograma);

    // početno postavljanje semafora
    sem_init(&(v->djed), 1, 0);
    sem_init(&(v->sobovi), 1, 0);
    sem_init(&(v->patuljci), 1, 0);
    sem_init(&(v->djeca), 1, 0);
    sem_init(&(v->KO), 1, 1);
    sem_init(&(v->glavni), 1, 0);

    // stvaranje novog procesa, i spremanje njegovog PID-a
    pid_t x = fork();

    switch(x) {
        
        // ako program nije uspio stvoriti proces sjeverng pola, mora završiti s radom
        case -1:
            printf("Nije bilo moguće stvoriti proces sjevernog pola!\n");
            exit(1);

        // ovo je proces sjevernog pola, i stoga skače u svoju funkciju
        case 0:
            sjeverniPol();
            exit(0);

        // default - program nastavlja s radom
    }

    // stvaranje novog procesa, i spremanje njegovog PID-a
    pid_t y = fork();

    switch(y) {

        // ako program nije uspio stvoriti proces djeda Božićnjaka, mora završiti s radom
        case -1:
            printf("Nije bilo moguće stvoriti proces Djeda Božićnjaka!\n");
            exit(1);

        // ovo je proces djeda Božićnjaka, i stoga skače u svoju funkciju
        case 0:
            djedBozicnjak();
            exit(0);

        // default - program nastavlja s radom
    }

    // spremanje PID-ova procesa sjeverni pol i djed božićnjak
    v->polPid = x;
    v->djedPid = y;

    // oznaka da kad svi procesi otpuste memorijski segment, treba ga uništiti
    shmctl(shmID, IPC_RMID, NULL);

    // kontrolni ispis kraja postavljanja okoline za izvođenje programa
    printf("Inicijalizacija je završena!\n");

    // čekanje semafora koji će označiti da je sve završilo i da glavni program može prestati s radom - ovime onemogućujemo programu da prerano završi
    sem_wait(&(v->glavni));
}