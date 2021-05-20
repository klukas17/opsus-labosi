#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/shm.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>

// redni broj zadnjeg zahtjeva
int redni_broj_zahtjeva = -1;

// slobodne memorijske lokacije
int* dostupni;

// zauzete memorijske lokacije
int* zauzeti;

// broj memorijskih lokacija
int* prostor;

// broj slobodnih memorijskih lokacija
int* slobodni;

// identifikacijski brojevi glavnog procesa i (potencijalnog) procesa generatora signala
pid_t *glavni, *gen;

// (potencijalni) semafor za pristup kritičnom odsječku
sem_t *KO;

// struktura u kojoj se čuvaju svi podaci za jedan memorijski blok, i dodatno pokazivač na idući blok (vezana lista)
struct blok {
    int indeks;
    int velicina;
    int pocetak;
    struct blok* sljedbenik;
};

// zastavica u kojoj čuvamo način rada (0 - manualni, 1 - proceduralni)
int zastavica;

// zastavica u kojoj čuvamo želimo li nasumično velik zahtjev ili želimo sami odrediti njegovu veličinu
int nasumicno;

// pseudonim za tip
typedef struct blok* blok_p;

// pokazivač na početni memorijski blok, tj. glava vezane liste
blok_p pocetni_blok = NULL;

// pokazivač na prvu rupu
blok_p pocetna_rupa = NULL;

// početak slobodnog prostora na kraju segmenta, inicijalno je 0
int prva_krajnja_lokacija = 0;

// funkcija koja ispisuje trenutnu konfiguraciju spremnika - stanje svake lokacije, vezanu listu memorijskih blokova, i broj praznih lokacija
void ispis_trenutne_konfiguracije() {

    // ispis graničnika
    printf("\n------------------------------------------------------------------------------------------------------\n");

   // ispis trenutne konfiguracije spremnika spremnika
    printf("Trenutna konfiguracija spremnika:\n");
    printf("|");
    for (int i = 0; i < *prostor; i++) {
        if (dostupni[i]) printf("-|");
        else printf("%d|", zauzeti[i]);
    }
    printf("\n\n");

    // ispis vezane liste memorijskih blokova
    printf("Vezana lista spremničkih blokova:\n");

    // glava vezane liste
    printf("glava -> ");

    //svi elementi vezane liste
    blok_p trenutni_blok = pocetni_blok;
    while (trenutni_blok) {
        printf("%d(%d) -> ", trenutni_blok->indeks, trenutni_blok->pocetak);
        trenutni_blok = trenutni_blok->sljedbenik;
    }

    // kraj vezane liste
    printf("null\n");

    // ispis vezane liste rupa između memorijskih blokova
    printf("\nVezana lista rupa između spremničkih blokova:\n");

    // glava vezane liste
    printf("glava -> ");

    //svi elementi vezane liste
    trenutni_blok = pocetna_rupa;
    while (trenutni_blok) {
        printf("(%d, %d) -> ", trenutni_blok->pocetak, trenutni_blok->velicina);
        trenutni_blok = trenutni_blok->sljedbenik;
    }

    // kraj vezane liste
    printf("null\n");

    // ispis broja slobodnih memorijskih lokacija
    printf("\nSlobodno je %d spremničkih lokacija.\n", *slobodni);

    // ispis graničnika
    printf("------------------------------------------------------------------------------------------------------\n\n");
}

// funkcija koja skuplja otpatke tj. uklanja fragmente među blokovima
void sakupljanje_otpadaka() {

    // kontrolni ispis
    printf("Skupljanje otpadaka.\n");

    // iniciranje trenutnog bloka
    blok_p trenutni_blok = pocetni_blok;

    // iniciramo varijablu koja označava kraj prethodnog bloka na 0
    int zadnji_kraj = 0;

    // za svaki blok ponavljamo sljedeće operacije
    while (trenutni_blok) {

        // delta označava broj lokacija između trentunog i prethodnog bloka
        int delta = trenutni_blok->pocetak - zadnji_kraj;

        // broj lokacija između je veći od nule i treba pomicati trenutni blok
        if (delta > 0) {

            // za svaku lokaciju u trenutnom bloku ponavljamo
            for (int i = 0; i < trenutni_blok->velicina; i++) {

                // označavamo da je lokacija zauzeta, te joj bilježimo bovi indeks
                zauzeti[trenutni_blok->pocetak + i - delta] = trenutni_blok->indeks;
                dostupni[trenutni_blok->pocetak + i - delta] = 0;
            }

            // označavamo novi početak trenutnog bloka
            trenutni_blok->pocetak = trenutni_blok->pocetak - delta;
        }

        // ažuriramo varijablu za kraj prethodnog bloka
        zadnji_kraj = trenutni_blok->pocetak + trenutni_blok->velicina;

        // pomičemo se na sljedeći blok
        trenutni_blok = trenutni_blok->sljedbenik;
    }

    // za svaku preostalu lokaciju ponavljamo
    for (int i = zadnji_kraj; i < *prostor; i++) {

        // označavamo da je lokacija dostupna, te joj indeks simbolički postavljamo na -1
        zauzeti[i] = -1;
        dostupni[i] = 1;
    }

    // iniciranje varijabli za brisanje svih čvorova u listi rupa
    blok_p trenutna_rupa = pocetna_rupa;
    blok_p rupa_za_brisanje = NULL;

    // za svaku rupu ponovi
    while (trenutna_rupa) {

        // dohvati sljedbenika i sačuvaj pokazivač na trenutnu rupu
        rupa_za_brisanje = trenutna_rupa;
        trenutna_rupa = trenutna_rupa->sljedbenik;

        // brisanje trenutne rupe
        free(rupa_za_brisanje);
    }

    // ažuriranje pokazivača na početnu rupu
    pocetna_rupa = NULL;

    // ažuriranje prve krajnje lokacije
    prva_krajnja_lokacija = *prostor - *slobodni;

    // kontrolni ispis
    printf("Kraj skupljanja otpadaka.\n");
}

// funkcija koja pokušava zauzeti neki memorijski blok
void zauzmi() {

    // proceduralni način rada - čekaj semafor
    if (zastavica) sem_wait(KO);

    // stvoren je novi zahtjev, uvećaj redni broj za jedan u odnosu na prethodni, i deklariraj varijablu velicina
    redni_broj_zahtjeva++;
    int velicina;

    // nasumično generiranje broja traženih memorijskih lokacija u intervalu [1,5]
    if (nasumicno) velicina = rand() % 5 + 1;

    // manualni unos veličine zahtjeva
    else scanf("%d", &velicina);

    // ispis o trenutnom zahtjevu za zauzimanje prostora
    printf("Zahtjev pod brojem %d za %d spremničkih mjesta.\n", redni_broj_zahtjeva, velicina);

     // zastavica za prekidanje petlje
    int pronaden = 0;

    // lokacija na koju će se program vratiti ako micanjem fragmenata uspije osloboditi prostor za novi zahtjev
    trazenje_mjesta:;

    // ne postoje rupe, lokacija ide na kraj bloka
    if (!pocetna_rupa) {

        // na kraju bloka postoji dovoljno mjesta za novi blok
        if (prva_krajnja_lokacija + velicina <= *prostor) {

            // smanjenje broja slobodnih lokacija
            *slobodni -= velicina;

            // oznaka da je pronađeno mjesto
            pronaden = 1;

            // ažuriranje polja s podacima
            for (int i = 0; i < velicina; i++) {
                dostupni[prva_krajnja_lokacija + i] = 0;
                zauzeti[prva_krajnja_lokacija + i] = redni_broj_zahtjeva;
            }

            // zauziamnje memorije na novi blok
            blok_p novi_blok = malloc(sizeof(struct blok));

            // iniciranje novog bloka
            novi_blok->pocetak = prva_krajnja_lokacija;
            novi_blok->velicina = velicina;
            novi_blok->indeks = redni_broj_zahtjeva;
            novi_blok->sljedbenik = NULL;

            // ažuriranje krajnje lokacije
            prva_krajnja_lokacija += velicina;

            // ovo je prvi blok u listi
            if (!pocetni_blok) {
                pocetni_blok = novi_blok;
            }

            // ovo nije prvi blok u listi
            else {

                // dodavanje novog bloka na kraj vezane liste blokova
                blok_p trenutni_blok = pocetni_blok;
                while (trenutni_blok) {
                    if (trenutni_blok->sljedbenik) trenutni_blok = trenutni_blok->sljedbenik;
                    else {
                        trenutni_blok->sljedbenik = novi_blok;
                        break;
                    }
                }
            }
        }
    }

    // potrebno je provjeriti sve rupe
    else {

        // najmanja rupa koju tražimo je veličine bloka
        int donja_granica = velicina;

        // najveća rupa
        int gornja_granica = velicina;

        // tražimo najveći blok
        blok_p trenutna_rupa = pocetna_rupa;
        blok_p prethodna_rupa = NULL;
        while (trenutna_rupa) {
            if (trenutna_rupa->velicina > gornja_granica) gornja_granica = trenutna_rupa->velicina;
            prethodna_rupa = trenutna_rupa;
            trenutna_rupa = trenutna_rupa->sljedbenik;
        }

        // provjeravamo rupe validne veličine počev od onih manjih
        for (int i = donja_granica; i <= gornja_granica && !pronaden; i++) {

            // krećemo od početne rupe i gledamo svaku rupu veličine i, dok ne nađemo prikladnu rupu
            trenutna_rupa = pocetna_rupa;
            prethodna_rupa = NULL;
            while (trenutna_rupa && !pronaden) {

                // prelazimo na sljedbenika ako veličina nije zadovoljavajuća
                while (trenutna_rupa && trenutna_rupa->velicina != i) {
                    prethodna_rupa = trenutna_rupa;
                    trenutna_rupa = trenutna_rupa->sljedbenik;
                }

                // došli smo do kraja liste
                if (!trenutna_rupa) break;

                if (trenutna_rupa->velicina >= donja_granica) {

                    // ažuriranje zastavice pronađen
                    pronaden = 1;

                    // ažuriranje polja s podacima
                    for (int i = 0; i < velicina; i++) {
                        zauzeti[trenutna_rupa->pocetak + i] = redni_broj_zahtjeva;
                        dostupni[trenutna_rupa->pocetak + i] = 0;
                    }

                    // stvaranje novog bloka
                    blok_p novi_blok = malloc(sizeof(struct blok));
                    
                    // postavljanje vrijednosti novog bloka
                    novi_blok->indeks = redni_broj_zahtjeva;
                    novi_blok->pocetak = trenutna_rupa->pocetak;
                    novi_blok->velicina = velicina;
                    novi_blok->sljedbenik = NULL;
                    
                    // popunjavamo cijelu rupu
                    if (trenutna_rupa->velicina == donja_granica) {

                        // popunjavamo prvu rupu
                        if (!prethodna_rupa) {
                            pocetna_rupa = trenutna_rupa->sljedbenik;
                            free(trenutna_rupa);
                        }

                        // popunjavamo rupu na sredini ili kraju
                        else {
                            prethodna_rupa->sljedbenik = trenutna_rupa->sljedbenik;
                            free(trenutna_rupa);
                        }
                    }

                    // popunjavamo samo prvi dio rupe
                    else {
                        
                        // smanjujemo rupu i pomičemo joj početak
                        trenutna_rupa->velicina -= velicina;
                        trenutna_rupa->pocetak += velicina;
                    }

                    // dodavanje novog bloka na prikladno mjesto
                    blok_p trenutni_blok = pocetni_blok;
                    while (trenutni_blok->sljedbenik && novi_blok->pocetak > trenutni_blok->sljedbenik->pocetak) {
                        trenutni_blok = trenutni_blok->sljedbenik;
                    }

                    // spremanje sljedbenika idućeg bloka u posebnu varijablu
                    blok_p sljedbenik_blok = trenutni_blok->sljedbenik;

                    // novi blok treba ići na početak
                    if (pocetni_blok->pocetak > novi_blok->pocetak) {
                        novi_blok->sljedbenik = pocetni_blok;
                        pocetni_blok = novi_blok;
                    }

                    // novi blok treba ići u sredinu
                    else if (sljedbenik_blok) {
                        trenutni_blok->sljedbenik = novi_blok;
                        novi_blok->sljedbenik = sljedbenik_blok;
                    } 
                    
                    // novi blok treba ići na kraj
                    else {
                        trenutni_blok->sljedbenik = novi_blok;
                    }

                    // ažuriranje varijable sa slobodnim lokacijama
                    *slobodni -= velicina;
                }
            }
        }

        // nije pronađena prikladna rupa, ali možemo smjestiti zahtjev na kraj bloka
        if (!pronaden && prva_krajnja_lokacija + velicina <= *prostor) {

            // stvaranje novog bloka
            blok_p novi_blok = malloc(sizeof(struct blok));
            
            // postavljanje vrijednosti novog bloka
            novi_blok->indeks = redni_broj_zahtjeva;
            novi_blok->pocetak = prva_krajnja_lokacija;
            novi_blok->velicina = velicina;
            novi_blok->sljedbenik = NULL;

            // ažuriranje vrijednosti prve krajnje lokacije
            prva_krajnja_lokacija += velicina;

            // ažuriranje zastavice da je mjesto pronađeno
            pronaden = 1;

            // ažuriranje varijable sa slobodnim lokacijama
            *slobodni -= velicina;

            // ažuriranje polja s podacima
            for (int i = 0; i < velicina; i++) {
                zauzeti[novi_blok->pocetak + i] = redni_broj_zahtjeva;
                dostupni[novi_blok->pocetak + i] = 0;
            }

            // traženje zadnjeg čvora liste
            blok_p trenutni_blok = pocetni_blok;
            blok_p prethodni_blok = NULL;
            while (trenutni_blok) {
                prethodni_blok = trenutni_blok;
                trenutni_blok = trenutni_blok->sljedbenik;
            }

            // dodavanje trenutnog čvora na kraj liste
            if (!prethodni_blok) pocetni_blok = novi_blok;
            else prethodni_blok->sljedbenik = novi_blok;
        }
    }
     
    // nije pronađena dovoljno velika rupa
    if (!pronaden) {

        // defragmentacijom se može osloboditi prostor
        if (*slobodni >= velicina) {
            sakupljanje_otpadaka();
            goto trazenje_mjesta;
        }

        // ne postoji dovoljno prostora za zahtjev
        else printf("Nije bilo moguće poslužiti zahtjev.\n");
    } 

    // ispis trenutne konfiguracije spremnika
    ispis_trenutne_konfiguracije();

    // proceduralni način rada - postavi semafor
    if (zastavica) sem_post(KO);
}

// funkcija koja nasumično bira zahtjev koji treba osloboditi
int oslobodi_nasumicnog() {

    // iniciranje varijable broja blokova
    int broj_blokova = 0;

    // izbroji blokove
    for (blok_p trenutni_blok = pocetni_blok; trenutni_blok; trenutni_blok = trenutni_blok->sljedbenik)
        broj_blokova++;

    // nasumični odabir jednog od blokova
    int redni_broj_bloka = rand() % broj_blokova;

    // iniciranje varijable za pronalazak memorijskog bloka
    int curr_indeks = 0;

    // pronalazak indeksa generiranog bloka
    blok_p trenutni_blok = pocetni_blok;
    while (curr_indeks != redni_broj_bloka) {
        trenutni_blok = trenutni_blok->sljedbenik;
        curr_indeks++;
    }

    // povratna vrijednost je indeks bloka koji treba obrisati
    return trenutni_blok->indeks;
}

// funkcija koja pokušava osloboditi neki memorijski blok
void oslobodi() {

    // proceduralni način rada - čekaj semafor
    if (zastavica) sem_wait(KO);

    // ne postoji niti jedan memorijski blok
    if (!pocetni_blok) {
        printf("Niti jedno spremničko mjesto nije zauzeto.\n\b");
        if (zastavica) sem_post(KO);
        return;
    }

    // dohvaćanje zahtjeva za brisanje
    printf("Oslobađanje zahtjeva. Koji zahtjev treba osloboditi?\n");
    int slobodan;

    // proceduralni način rada - nasumično odaberi jedan memorijski blok
    if (zastavica) {
        slobodan = oslobodi_nasumicnog();
        printf("%d\n", slobodan);
    }

    // manualni način rada - upiši indeks memorijskog bloka
    else scanf("%d", &slobodan);
    
    // pronalazak traženog bloka
    blok_p trenutni_blok;
    blok_p prev_blok;
    for (trenutni_blok = pocetni_blok, prev_blok = NULL; 
            trenutni_blok && trenutni_blok->indeks != slobodan; 
            prev_blok = trenutni_blok, trenutni_blok = trenutni_blok->sljedbenik);
    
    // blok je pronađen
    if (trenutni_blok && trenutni_blok->indeks == slobodan) {

        // uvećaj broj slobodnih blokova
        *slobodni += trenutni_blok->velicina;

        // spremanje podataka o veličini i početnoj adresi bloka
        int velicina = trenutni_blok->velicina;
        int start = trenutni_blok->pocetak;

        // ne oslobađamo prvi blok
        if (prev_blok) {
            prev_blok->sljedbenik = trenutni_blok->sljedbenik;
        }

        // oslobađamo prvi blok
        else {
            pocetni_blok = trenutni_blok->sljedbenik;
        }

        // brisanje indeksa trenutnog bloka jer je postao rupa
        trenutni_blok->indeks = -1;
        
        // dodavanje obrisanog bloka u listu rupa
        // ne postoji niti jedna rupa i trenutni blok nije zadnji
        if (!pocetna_rupa && trenutni_blok->sljedbenik) {
             pocetna_rupa = trenutni_blok;

             // brisanje sljedbenika trenutnog bloka
             trenutni_blok->sljedbenik = NULL;
        }

        // već postoje rupe, potrebno je prikladno smjestiti novonastalu rupu u listu
        else if (trenutni_blok->sljedbenik) {

            // brisanje sljedbenika trenutnog bloka
            trenutni_blok->sljedbenik = NULL;

            // novu rupu treba dodati na početak vezane liste
            if (pocetna_rupa->pocetak > trenutni_blok->pocetak) {
                trenutni_blok->sljedbenik = pocetna_rupa;
                pocetna_rupa = trenutni_blok;
            }

            // novu rupu treba dodati na sredinu ili kraj vezane liste
            else {

                // zastavica za provjeru uvjeta
                int zastava = 1;

                // traženje prethodnika nove rupe
                blok_p trenutna_rupa = pocetna_rupa;
                blok_p prethodna_rupa = NULL;
                while (trenutna_rupa && trenutna_rupa->pocetak + trenutna_rupa->velicina <= trenutni_blok->pocetak) {

                    // trenutna rupa nema sljedbenika
                    if (!trenutna_rupa->sljedbenik) {
                        trenutna_rupa->sljedbenik = trenutni_blok;
                        zastava = 0;
                        break;
                    }

                    // trenutna rupa ima sljedbenika
                    else {
                        prethodna_rupa = trenutna_rupa;
                        trenutna_rupa = trenutna_rupa->sljedbenik;
                    } 
                }

                // prespajanje nismo odradili unutar petlje
                if (zastava) {

                    // prespajanje pokazivača
                    trenutni_blok->sljedbenik = prethodna_rupa->sljedbenik;
                    prethodna_rupa->sljedbenik = trenutni_blok;
                }
            }
        }

        // obrisan je zadnji blok
        else {
            
            // smanjujemo početak praznog prostora
            prva_krajnja_lokacija -= trenutni_blok->velicina;

            // brisanje trenutnog bloka
            free(trenutni_blok);

            // brisanje posljednje rupe, ukoliko je spojena sa sada praznim krajnjim prostorom
            blok_p trenutna_rupa = pocetna_rupa;
            blok_p prethodna_rupa = NULL;

            // pronalazak posljednje rupe
            while (trenutna_rupa && trenutna_rupa->sljedbenik) {
                prethodna_rupa = trenutna_rupa;
                trenutna_rupa = trenutna_rupa->sljedbenik;
            }

            // potrebno je spojiti zadnju rupu s praznim prostorom
            if (trenutna_rupa && trenutna_rupa->pocetak + trenutna_rupa->velicina == prva_krajnja_lokacija) {

                // brišemo sljedbenika prethodne rupe, ili početnu rupu postavljamo na NULL
                if (prethodna_rupa) {
                    prethodna_rupa->sljedbenik = NULL;
                } else pocetna_rupa = NULL;

                // pomičemo granicu unazad
                prva_krajnja_lokacija -= trenutna_rupa->velicina;

                // oslobađanje memorije za trenutnu rupu
                free(trenutna_rupa);
            }
            
        }

        // označavanje svih oslobođenih memorijskih lokacija
        for (int i = 0; i < velicina; i++) {
            dostupni[start + i] = 1;
            zauzeti[start + i] = -1;
        }

        // spajanje rupa
        blok_p trenutna_rupa = pocetna_rupa;

        // prolazimo kroz svaku rupu i ponavljamo sljedeće
        while (trenutna_rupa && trenutna_rupa->sljedbenik) {

            // potrebno je spajati trenutnu rupu sa njenim sljedbenikom
            while (trenutna_rupa && trenutna_rupa->sljedbenik && trenutna_rupa->pocetak + trenutna_rupa->velicina == trenutna_rupa->sljedbenik->pocetak) {

                // dohvaćanje rupe za brisanje
                blok_p rupa_za_brisanje = trenutna_rupa->sljedbenik;

                // prespajanje pokazivača
                trenutna_rupa->velicina += rupa_za_brisanje->velicina;
                trenutna_rupa->sljedbenik = rupa_za_brisanje->sljedbenik;

                // oslobađanje prostora rupa ze brisanje
                free(rupa_za_brisanje);
            }

            // prelazak na sljedbenika trenutne rupe
            if (trenutna_rupa) trenutna_rupa = trenutna_rupa->sljedbenik;
        } 

       // ispis trenutne konfiguracije spremnika
        ispis_trenutne_konfiguracije();
    } 
    
    // zahtjev nije aktivan
    else {
        printf("Zahtjev koji želite osloboditi nije aktivan.\n\n");

       // ispis trenutne konfiguracije spremnika
        ispis_trenutne_konfiguracije();
    }

    // proceduralni način rada - postavi semafor
    if (zastavica) sem_post(KO);
}

// funkcija koja kroz svoj proces glavnom procesu šalje signale SIGUSR1 i SIGUSR2
void generator() {

    // lokalne varijable
    int sansa, zauzimanje, oslobadanje;

    // početno postavljanje vjerojatnosti idućeg događaja
    zauzimanje = *slobodni;
    oslobadanje = *prostor - *slobodni;

    // pauziranje programa
    usleep(1000);

    // u beskonačnoj petlji generiramo SIGUSR1 i SIGUSR2 signale i šaljemo ih glavnom programu
    while (1) {

        // računanje vjerojatnosti za idući događaj
        sansa = rand() % *prostor;

        // slanje signala za zauzimanje memorije
        if (sansa <= zauzimanje) {

            // čekaj semafor
            sem_wait(KO);

            // slanje signala SIGUSR1 glavnom programu
            kill(*glavni, SIGUSR1);

            // postavi semafor
            sem_post(KO);

            // pauziranje programa
            usleep(1000);

            // ponovno računanje lokalnih varijabli
            zauzimanje = *slobodni;
            oslobadanje = *prostor - *slobodni;
        }

        // ažuriranje vjerojatnosti idućih događaja
        sansa = rand() % *prostor;

        // slanje signala za brisanje memorije
        if (sansa <= oslobadanje) {

            // čekaj semafor
            sem_wait(KO);

            // slanje signala SIGUSR2 glavnom programu
            kill(*glavni, SIGUSR2);

            // postavi semafor
            sem_post(KO);

            // pauziranje programa
            usleep(1000);

            // ažuriranje vjerojatnosti idućih događaja
            zauzimanje = *slobodni;
            oslobadanje = *prostor - *slobodni;
        }
    }
}

// prekidna rutina za kraj programa
void kraj() {

    // kontrolni ispis
    printf("\nKraj simulacije.\n");

    // dohvaćanje identifikacijskog broja drugog procesa i ubijanje njega
    if (getpid() == *glavni) {
        kill(*gen, SIGKILL);
    } else {
        kill(*glavni, SIGKILL);
    }

    // ubijanje sebe
    exit(0);
}

// glavna funkcija u kojoj postavljamo okolinu i potom beskoančno čitamo znakove ili čekamo signale, ovisno o načinu rada
void main(int argc, char *argv[]) {

    // argument koji označava broj raspoloživih spremničkih lokacija
    int val = atoi(argv[1]);

    // unos željenog načina rada
    printf("Želite li sami generirati zahtjeve (Y) ili želite da se oni nasumično generiraju (N)? (Y/N): ");
    char mode = getchar();
    if (mode == 'Y') {
        zastavica = 0;
    } else if (mode == 'N') {
        zastavica = 1;
    } else {
        printf("Niste odabrali znak Y ili N!\n");
        exit(1);
    }

    // postavljanje seeda za generiranje nasumičnih brojeva
    srand(time(NULL));

    // proceduralni način rada - treba postaviti okolinu za paralelno izvođenje dva procesa
    if (zastavica) {

        // dohvaćanje memorijskog segmenta
        int shmID = shmget(IPC_PRIVATE, 2 * sizeof(pid_t) + 2 * sizeof(int) + sizeof(sem_t), 0600);

        // nije bilo moguće dohvatiti memorijski segment
        if (shmID == -1) {
            printf("Nije moguće stvoriti zajednički memorijski segment.\n");
            exit(1);
        }

        // smještanje svih podataka na memorijski segment
        glavni = (pid_t*) (shmat(shmID, NULL, 0));
        gen = (pid_t*) glavni + 1;
        slobodni = (int*) gen + 1;
        prostor = (int*) slobodni + 1;
        KO = (sem_t*) prostor + 1;

        // iniciranje semafora za pristup zajedničkim varijablama tj. kritičnom odsječku
        sem_init(KO, 1, 1);

        // dohvaćanje identifikacijskog broja glavnog procesa
        *glavni = getpid();

        // placeholder za identifikacijski broj generatorskog procesa
        *gen = 0;

        // iniciranje varijabli prostor (broj lokacija) i slobodni (broj slobodnih lokacija) na broj lokacija
        *prostor = val;
        *slobodni = val;
        pid_t pid_djeteta;

        // postavljanje prekidne rutine za kraj programa
        sigset(SIGINT, kraj);

        // stvaranje generatorskog procesa
        switch(pid_djeteta = fork()) {

            // nemoguće stvoriti generatorski proces
            case -1:
                printf("Nije bilo moguće stvoriti novi proces.\n");
                exit(1);

            // ovo je generatorski proces
            case 0:
                generator();
                exit(0);

            // ovo je glavni proces
            default:
                
                // spremanje identifikacijskog broja djeteta (generatorski proces)
                *gen = pid_djeteta;

                // postavljanje prekidne rutine za zauzimanje i oslobađanje memorije
                sigset(SIGUSR1, zauzmi);
                sigset(SIGUSR2, oslobodi);
        }

        // označavanje da će svi zahtjevi biti generirani nasumično
        nasumicno = 1;

        // označavanje memorijskog segmenta da je za brisanje, pa će ga OS obrisati kad on izgubi vezu sa svim procesima
        shmctl(shmID, IPC_RMID, NULL);
    }

    // manualni način rada - treba inicirati globalne varijable
    if (!zastavica) {
        prostor = malloc(sizeof(int));
        slobodni = malloc(sizeof(int));
        *prostor = *slobodni = val;
    }

    // zauzimanje memorije za slobodna i zauzeta mjesta
    dostupni = malloc(*prostor * sizeof(int));
    zauzeti = malloc(*prostor * sizeof(int));

    // iniciranje početnih vrijednosti za slobodna i zauzeta mjesta
    for (int i = 0; i < *prostor; i++) {
        dostupni[i] = 1;
        zauzeti[i] = -1;
    }

    // ispis početne konfiguracije spremnika
    ispis_trenutne_konfiguracije();

    // manualni način rada - u beskonačnoj petlji čitamo zahtjeve za zauzimanje ili oslobađanje memorije
    while(!zastavica) {

        // čitanje znaka i čekanje do znaka Z, O, S ili K
        char c = getchar();
        while (c != 'Z' && c != 'O' && c != 'S' && c != 'K') c = getchar();

        // zauzimanje memorije
        if (c == 'Z') {
            nasumicno = 1;
            zauzmi();
        }

        if (c == 'K') {
            nasumicno = 0;
            zauzmi();
        }

        // oslobađanje memorije
        if (c == 'O') {
            oslobodi();
        }

        // skupljanje otpadaka
        if (c == 'S') {
            sakupljanje_otpadaka();
            ispis_trenutne_konfiguracije();
        }
    }

    // proceduralni način rada - u beskonačnoj petlji čekamo signal (definirano ponašanje za SIGUSR1, SIGUSR2 i SIGINT)
    while (zastavica) {
        pause();
    }
}