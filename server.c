#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h> 
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <getopt.h>
#include <signal.h>

#include <sys/socket.h> 
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#include "header/server.h"
#include "header/shared.h"
#include "header/macros.h"
#include "header/liste.h"
#include "header/trie.h"
#include "header/bacheca.h"
#include "header/matrice.h"

// variabili globali

pthread_t main_tid;
pthread_t scorer_tid;
pthread_t gioco_tid;

// socket
int fd_server;

// parametri con valori di default
char *data_filename = NULL;

// ricambialo a 3
int durata_gioco = 1;

int rnd_seed;

char *dizionario = "dictionary_ita.txt";

int durata_pausa = 1;

// variabile globale per il tempo di inizio del gioco
time_t tempo_gioco, tempo_pausa;

// variabile globale per capire se il gioco è in corso o è in pausa
// 0 = pausa, 1 = gioco
int fase_gioco = 0;

// variabile globale per determinare quando la partita finisce, utile a risvegliare lo scorer
// 0 = partita in corso, 1 = partita finita
int partita_finita = 0;

// variabile per memorizzare quanti hanno inviato il messaggio di chiusura al proprio client
int num_chiusure = 0;


// strutture dati globali

Trie *radice = NULL;

Messaggio *bacheca;
int n_post = 0;

char **matrice;

lista_thread *Threads;

lista_thread_handler *Thread_Handler;

lista_giocatori *Giocatori;

coda_risultati *Punteggi;

// array globale per memorizzare temporaneamente i dati della coda
punteggi tmp[MAX_CLIENT];

// classifica
char classifica[MAX_DIM];

// mutex
pthread_mutex_t client_mtx;
pthread_mutex_t handler_mtx;
pthread_mutex_t giocatori_mtx;
pthread_mutex_t parole_mtx;

pthread_mutex_t scorer_mtx;

pthread_mutex_t bacheca_mtx;

pthread_mutex_t fase_mtx;

pthread_mutex_t sig_mtx;

// condition variable
pthread_cond_t giocatori_cond;
pthread_cond_t scorer_cond;
pthread_cond_t fase_cond;
pthread_cond_t sig_cond;

// gestione dei segnali

// inizializzazione dei segnali centralizzata
void inizializza_segnali() {
    struct sigaction sa_sigalrm, sa_sigusr2;
    sigset_t set;

    // inizializzazione della maschera
    sigemptyset(&set);

    // bloccare sigusr2 globalmente
    sigaddset(&set, SIGUSR2);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    // configurazione di SIGALRM
    sa_sigalrm.sa_handler = sigalrm_handler; // viene gestito dall'handler
    sa_sigalrm.sa_mask = set;
    sa_sigalrm.sa_flags = SA_RESTART;      // per riavviare le chiamate interrotte

    SYST(sigaction(SIGALRM, &sa_sigalrm, NULL));

    // configurazione di SIGUSR2
    sa_sigusr2.sa_handler = sigusr2_handler;
    sigemptyset(&sa_sigusr2.sa_mask);
    sa_sigusr2.sa_flags = SA_RESTART;

    SYST(sigaction(SIGUSR2, &sa_sigusr2, NULL));
}


// thread per gestire il segnale di chiusura del gioco
// consumatore sulla variabile delle chiusure
// avvia la procedura di pulizia e chiusura
void *sigint_handler (void *args) {

    pthread_mutex_lock(&sig_mtx);

    // attesa fino a quando il numero di chiusure è minore del numero di thread attivi
    while (num_chiusure < Threads -> num_thread) {
        pthread_cond_wait(&sig_cond, &sig_mtx);
    }

    pthread_mutex_unlock(&sig_mtx);

    // ora può avviare la procedura di chiusura

    // chiudere il gioco
    SYST(pthread_cancel(gioco_tid));

    // chiudere lo scorer
    SYST(pthread_cancel(scorer_tid));

    // eliminare la lista dei thread handler
    if (Thread_Handler) {
        svuota_lista_handler(Thread_Handler);
        free(Thread_Handler);
    }

    // eliminare la lista delle parole per ogni giocatore
    giocatore *tmp_g = Giocatori -> head;
    while (tmp_g != NULL) {
        if (tmp_g -> parole_trovate) {
            svuota_lista_parole(tmp_g -> parole_trovate);
            free(tmp_g -> parole_trovate);
        }
        tmp_g = tmp_g -> next;
    }

    free(tmp_g);

    // eliminare la lista dei giocatori
    if (Giocatori) {
        svuota_lista_giocatori(Giocatori);
        free(Giocatori);
    }

    // liberare la coda punteggi se era stata riempita
    if (Punteggi -> num_risultati > 0) {
        svuota_coda_risultati(Punteggi);
        free(Punteggi);
    }

    // chiudere i thread client
    thread_attivo *tmp = Threads -> head;
    while (tmp != NULL) {
        SYST(pthread_cancel(tmp -> t_id));
        tmp = tmp -> next;
    }

    free(tmp);

    // eliminare la lista
    if (Threads) {
        svuota_lista_thread(Threads);
        free(Threads);
    }

    SYST(pthread_mutex_destroy(&client_mtx));
    SYST(pthread_mutex_destroy(&handler_mtx));
    SYST(pthread_mutex_destroy(&giocatori_mtx));
    SYST(pthread_mutex_destroy(&parole_mtx));
    SYST(pthread_mutex_destroy(&scorer_mtx));
    SYST(pthread_mutex_destroy(&bacheca_mtx));
    SYST(pthread_mutex_destroy(&fase_mtx));
    SYST(pthread_mutex_destroy(&sig_mtx));

    SYST(pthread_cond_destroy(&giocatori_cond));
    SYST(pthread_cond_destroy(&scorer_cond));
    SYST(pthread_cond_destroy(&sig_cond));

    // deallocazione bacheca
    if (bacheca) {
        deallocazione_bacheca(bacheca, &n_post);
    }

    // deallocazione matrice
    if (matrice) {
        deallocazione_matrice(matrice);
    }

    // deallocazione trie
    if (radice) {
        deallocazione_trie(radice);
    }

    // chiudere il socket
    close(fd_server);
    SYST(pthread_cancel(main_tid));

    pthread_exit(NULL);
}

// thread per la gestione dei segnale per i client 
// produttori sulla variabile per le chiusure e sulla coda dei punteggi
void *sigclient_handler (void *args) {
    sigset_t* set = (sigset_t*)args;

    int sig;

    // sblocco del segnale nel thread
    SYST(pthread_sigmask(SIG_UNBLOCK, set, NULL));

    while (1) {
        SYST(sigwait(set, &sig));

        if (sig == SIGINT) {
            // creazione del thread per gestire SIGINT
            sigset_t set;
            sigemptyset(&set);
            sigaddset(&set, SIGINT);

            // blocco sigint a tutti
            SYST(pthread_sigmask(SIG_BLOCK, &set, NULL));

            pthread_t sigint_tid;
            // creazione del thread per gestire SIGINT
            SYST(pthread_create(&sigint_tid, NULL, sigint_handler, NULL));

            // alla ricezione del segnale, il thread deve inviare il messaggio di chiusura del server al proprio client
            pthread_mutex_lock(&giocatori_mtx);

            // recuperare il fd
            int fd_c = recupera_fd_2(Giocatori, pthread_self());

            pthread_mutex_unlock(&giocatori_mtx);

            char *msg = "Il server sta chiudendo, disconnessione in corso";

            prepara_msg(fd_c, MSG_SERVER_SHUTDOWN, msg);

            // comunicare sulla variabile condivisa che il messaggio è stato inviato
            pthread_mutex_lock(&sig_mtx);
            num_chiusure++;
            pthread_cond_signal(&sig_cond);
            pthread_mutex_unlock(&sig_mtx);

            // il thread ha finito ciò che doveva fare -> terminazione
            pthread_exit(NULL);
        }
        else if (sig == SIGUSR1) {
            // alla ricezione del segnale, il thread deve mettere il punteggio del giocatore sulla coda condivisa
            
            // recuperare username e punteggio dalla lista dei giocatori tramite il tid del thread
            // viene acquisita la mutex sulla lista giocatori
            pthread_mutex_lock(&giocatori_mtx);

            char *username = recupera_username(Giocatori, pthread_self());
            int punteggio = recupera_punteggio(Giocatori, pthread_self());

            // resettare il punteggio del giocatore per prepararsi alla prossima partita
            resetta_punteggio(Giocatori, pthread_self());

            // rilascio della mutex sulla lista giocatori
            pthread_mutex_unlock(&giocatori_mtx);

            // acquisizione della mutex sulla coda
            pthread_mutex_lock(&scorer_mtx);

            // inserimento del punteggio nella coda
            inserisci_risultato(Punteggi, username, punteggio);

            // lo scorer viene avvisato che è stato scritto un punteggio sulla coda
            pthread_cond_signal(&scorer_cond);

            // rilascio della mutex
            pthread_mutex_unlock(&scorer_mtx);
        }
    }

    return NULL;
}

// segnale per gestione dei tempi e delle fasi del gioco
void sigalrm_handler (int sig) {
    // se è finita la partita, mettere il gioco in pausa
    // mandare il segnale SIGUSR1 a sigclient handler e allo scorer
    if (fase_gioco == 1) {
        pthread_mutex_lock(&fase_mtx);
        fase_gioco = 0;
        pthread_mutex_unlock(&fase_mtx);

        printf("GIOCO FINITOOOOO!!! \n");

        // invio del segnale ai thread handler
        /*if (Threads -> num_thread > 0) {
            pthread_mutex_lock(&handler_mtx);
            invia_sigusr1(Thread_Handler, SIGUSR1);
            pthread_mutex_unlock(&handler_mtx);
        }*/

        // avviso lo scorer per farlo risvegliare
        /*pthread_mutex_lock(&scorer_mtx);
        partita_finita = 1;
        pthread_cond_signal(&scorer_cond);
        pthread_mutex_unlock(&scorer_mtx);*/
    }
    else {
        // se è finita la pausa, rimettere il gioco in corso
        pthread_mutex_lock(&fase_mtx);
        fase_gioco = 1;
        pthread_cond_signal(&fase_cond);
        pthread_mutex_unlock(&fase_mtx);
    }
}

void sigusr2_handler (int sig) {
    // invio della classifica al client associato
    if (sig == SIGUSR2) {
        pthread_mutex_lock(&giocatori_mtx);
        int fd_c = recupera_fd(Giocatori, pthread_self());
        pthread_mutex_unlock(&giocatori_mtx);

        prepara_msg(fd_c, MSG_PUNTI_FINALI, classifica);
    }
}


// funzioni

// caricamento del dizionario nel trie
void caricamento_dizionario(char *file_dizionario, Trie *radice) {

    FILE *fp = fopen(file_dizionario, "r");

    if (fp == NULL) {
        perror("Errore nell'apertura del file dizionario");
        exit(EXIT_FAILURE);
    }
    else {
        char tmp_buffer[256];

        while (fscanf(fp, "%s", tmp_buffer) != EOF) {
            inserimento_trie(radice, tmp_buffer);
        }
    }

    fclose(fp);
}

// calcolo del tempo rimanente
char *tempo_rimanente(time_t tempo, int minuti) {
    time_t tempo_attuale = time(NULL);

    double tempo_trascorso = difftime(tempo_attuale, tempo);

    // conversione in secondi
    int in_secondi = minuti * 60;

    // calcolo del tempo rimanente
    double rimanente = in_secondi - tempo_trascorso;

    // restituire come stringa (per il messaggio)
    char *t;
    SYSCN(t, (char*)malloc(50 * sizeof(char)), "Errore nell'allocazione della stringa per il tempo rimanente");

    sprintf(t, "%d", (int)rimanente);

    return t;
}

int sorting_classifica(const void *a, const void *b) {
    return ((punteggi*)b) -> punteggio - ((punteggi*)a) -> punteggio;
}

// funzione per la gestione del client
// si occupa sia di invio che di ricezione
void *thread_client (void *args) {

    client_args *params = (client_args*)args;
    int fd_c = params -> sck;

    // giocatore associato al thread
    giocatore *player = NULL;

    lista_parole *Parole_Trovate = inizializza_parole();

    // variabile per ricevere il messaggio
    Msg_Socket *richiesta = NULL;

    char *tempo = NULL;

    // maschera dei segnali da dare al thread sigclient_handler
    sigset_t set;
    sigemptyset(&set);

    // aggiungere SIGINT e SIGUSR1 alla maschera
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGUSR1);
    // blocco SIGUSR1 per thread_client, viene sbloccato successivamente nel thread handler
    SYST(pthread_sigmask(SIG_BLOCK, &set, NULL));

    // alloco una stringa per la matrice
    // (*2 per lo spazio tra le lettere)
    char *matrice_strng;
    size_t dim = MAX_CASELLE * MAX_CASELLE * 2;
    SYSCN(matrice_strng, malloc(dim * sizeof(char)), "Errore nell'allocazione della stringa per la matrice");

    char *bacheca_strng = NULL;

    printf("ciao sono thread client tid: %ld \n", pthread_self());

    // tutto il processo di gestione del client viene eseguito fino a quando non si chiude il server
    
    while (1) {
        // prima della registrazione, si accettano solo messaggi di registrazione e di fine
        // (messaggio di aiuto viene gestito nel client)
        richiesta = ricezione_msg(fd_c);

        printf("tipo messaggio: %c \n", richiesta -> type);
        printf("messaggio ricevuto: %s \n", richiesta -> data);

        if (richiesta -> type == MSG_CLIENT_SHUTDOWN) {
            // se il client invia "fine"
            // eliminare il thread dalla lista dei thread
            pthread_mutex_lock(&client_mtx);
            rimuovi_thread(Threads, pthread_self());
            pthread_mutex_unlock(&client_mtx);
            
            // deallocare la struct dei suoi parametri
            free(params);

            // dealloca tutta la memoria allocata fino ad ora
            free(matrice_strng);
            free(Parole_Trovate);
            
            // terminazione del thread
            pthread_exit(NULL);
        }
        else if (richiesta -> type == MSG_REGISTRA_UTENTE) {
            // controllo lunghezza e caratteri già fatto nel client
            char *nome_utente = richiesta -> data;

            // se la lista giocatori è piena inviare messaggio di errore
            if (Giocatori -> num_giocatori == MAX_CLIENT) {
                char *msg = "Numero massimo di giocatori raggiunto, rimani in attesa per giocare";
                prepara_msg(fd_c, MSG_ERR, msg);
                continue;
            }
            // se il nome utente è già presente nella lista giocatori inviare messaggio di errore
            else if (cerca_giocatore(Giocatori, nome_utente) == 1) {
                char *msg = "Nome utente già in uso, scegline un altro";
                prepara_msg(fd_c, MSG_ERR, msg);
                continue;
            }

            // se il nome utente è valido, inizializzare il giocatore
            pthread_mutex_lock(&giocatori_mtx);
            player = inserisci_giocatore(Giocatori, nome_utente, fd_c);
            player -> parole_trovate = Parole_Trovate;

            Giocatori -> num_giocatori++;

            // signal per avvisare il gioco di partire
            pthread_cond_signal(&giocatori_cond);
            printf("segnalato gioco \n");

            pthread_mutex_unlock(&giocatori_mtx);

            // stampa lista thread
            printf("quali thread ci sono fino ad ora? \n");
            pthread_mutex_lock(&client_mtx);
            thread_attivo *tmp_tt = Threads -> head;
            while (tmp_tt != NULL) {
                printf("thread: %ld \n", tmp_tt -> t_id);
                tmp_tt = tmp_tt -> next;
            }
            pthread_mutex_unlock(&client_mtx);
            free(tmp_tt);

            pthread_mutex_lock(&giocatori_mtx);
            giocatore *tmp_gg = Giocatori -> head;
            while (tmp_gg != NULL) {
                printf("giocatore: %s \n", tmp_gg -> username);
                tmp_gg = tmp_gg -> next;
            }
            pthread_mutex_unlock(&giocatori_mtx);
            free(tmp_gg);

            // inviare messaggio di avvenuta registrazione
            char *msg = "Registrazione avvenuta con successo, sei pronto a giocare?";
            prepara_msg(fd_c, MSG_OK, msg);
            break;
        }
        else if (richiesta -> data == NULL || richiesta -> type != MSG_REGISTRA_UTENTE || richiesta -> type != MSG_CLIENT_SHUTDOWN) {
            // messaggio non valido
            char *msg = "Messaggio non valido";
            prepara_msg(fd_c, MSG_ERR, msg);
            continue;
        }

        // pulire le variabili appena utilizzate per lo scambio di messaggi di questa fase
        free(richiesta -> data);
        free(richiesta);
    }

    // creazione del thread handler per i segnali
    pthread_t tid_sigclient;
    SYST(pthread_create(&tid_sigclient, NULL, sigclient_handler, (void*)&set));

    // aggiunta del thread appena creato alla lista
    pthread_mutex_lock(&handler_mtx);
    inserisci_handler(Thread_Handler, tid_sigclient, pthread_self());
    pthread_mutex_unlock(&handler_mtx);

    // aggiornamento del parametro del giocatore
    pthread_mutex_lock(&giocatori_mtx);
    player -> tid_sigclient = tid_sigclient;
    pthread_mutex_unlock(&giocatori_mtx);

    // per ripulire la maschera
    sigemptyset(&set);

    // ora si sblocca SIGUSR2 per thread_client
    sigaddset(&set, SIGUSR2);
    SYST(pthread_sigmask(SIG_UNBLOCK, &set, NULL));

    // invio della matrice e del tempo
    // se è il primo ad entrare non gli viene inviata perché tanto gliela invierà il gioco appena entrerà ?????
    if (fase_gioco == 0) {
        // gioco in pausa -> invio del tempo di attesa
        tempo = tempo_rimanente(tempo_pausa, durata_pausa);

        prepara_msg(fd_c, MSG_TEMPO_ATTESA, tempo);
    }
    else {
        // gioco in corso -> invio matrice e tempo rimanente
        matrice_strng = matrice_a_stringa(matrice, matrice_strng);

        prepara_msg(fd_c, MSG_MATRICE, matrice_strng);

        tempo = tempo_rimanente(tempo_gioco, durata_gioco);

        prepara_msg(fd_c, MSG_TEMPO_PARTITA, tempo);  
    }

    // ciclo di ascolto durante il gioco
    while (1) {

        // appena sono in pausa svuoto la lista di parole
        while (fase_gioco == 0 && player -> parole_trovate -> num_parole > 0) {
            pthread_mutex_lock(&parole_mtx);
            svuota_lista_parole(player -> parole_trovate);
            pthread_mutex_unlock(&parole_mtx);
        }

        richiesta = ricezione_msg(fd_c);

        if (richiesta -> type == MSG_CLIENT_SHUTDOWN) {
            // se il client invia "fine"
            // eliminare il thread dalla lista dei thread
            pthread_mutex_lock(&client_mtx);
            rimuovi_thread(Threads, pthread_self());
            pthread_mutex_unlock(&client_mtx);

            // deallocare la struct dei suoi parametri
            free(params);

            // eliminare il giocatore dalla lista dei giocatori
            pthread_mutex_lock(&giocatori_mtx);
            rimuovi_giocatore(Giocatori, player -> username);
            pthread_mutex_unlock(&giocatori_mtx);

            // eliminare lista parole trovate
            pthread_mutex_lock(&parole_mtx);
            svuota_lista_parole(player -> parole_trovate);
            pthread_mutex_unlock(&parole_mtx);
            
            // terminazione del thread
            pthread_exit(NULL);
        }
        else if (richiesta -> type == MSG_PAROLA) {
            if (fase_gioco == 1) {

                // controllo della lunghezza è stato già fatto nel client
                char *p = richiesta -> data;

                // controllare che il giocatore non abbia già inserito la parola
                if (cerca_parola(player -> parole_trovate, p) == 1) {
                    // se è già presente nella lista, inviare punti parola a 0
                    char *msg = "0";
                    prepara_msg(fd_c, MSG_PUNTI_PAROLA, msg);
                    continue;
                }
                // controllare che sia presente nel dizionario e sia componibile nella matrice
                else if (ricerca_trie(radice, p) == 0) {
                    // se non è presente nel dizionario inviare messaggio di errore
                    char *msg = "Parola non presente nel dizionario";
                    prepara_msg(fd_c, MSG_ERR, msg);
                    continue;
                }
                    
                int len = strlen(p);
                char *par = malloc(len + 1);
                int i = 0, j = 0;

                // prima di ricercare nella matrice togliere eventuali occorrenze di "qu"
                if (strstr(p, "qu")) {
                    while (i < len) {
                        if (p[i] == 'q' && p[i + 1] == 'u') {
                            par[j] = 'q';
                            i += 2;
                        }
                        else {
                            par[j] = p[i];
                            i++;
                        }
                        j++;
                    }            
                }

                par[j] = '\0';

                strcpy(p, par);
                free(par);

                if (ricerca_matrice(matrice, p) == 0) {
                    char *msg = "Parola non presente nella matrice";
                    prepara_msg(fd_c, MSG_ERR, msg);
                    continue;
                }

                // se la parola è valida, inviare i punti
                int punti = strlen(p);

                // inserire la parola nella lista delle parole trovate
                pthread_mutex_lock(&parole_mtx);
                inserisci_parola(player -> parole_trovate, p, punti);
                pthread_mutex_unlock(&parole_mtx);

                // incrementare il punteggio del giocatore
                pthread_mutex_lock(&giocatori_mtx);
                player -> punteggio += punti;
                pthread_mutex_unlock(&giocatori_mtx);

                char *msg;
                SYSCN(msg, malloc(12 * sizeof(char)), "Errore nell'allocazione della stringa per i punti");

                snprintf(msg, 12, "%d", punti);

                prepara_msg(fd_c, MSG_PUNTI_PAROLA, msg);

                free(msg);
                continue;                
            }
            else {
                // inviare messaggio di errore
                char *msg = "Il gioco è in pausa, ora non puoi inviare parole";
                prepara_msg(fd_c, MSG_ERR, msg);
                continue;
            }
        }
        else if (richiesta -> type == MSG_MATRICE) {
            if (fase_gioco == 0) {
                tempo = tempo_rimanente(tempo_pausa, durata_pausa);
                prepara_msg(fd_c, MSG_TEMPO_ATTESA, tempo);
                continue;
            }
            else {
                matrice_strng = matrice_a_stringa(matrice, matrice_strng);

                prepara_msg(fd_c, MSG_MATRICE, matrice_strng);

                tempo = tempo_rimanente(tempo_gioco, durata_gioco);

                prepara_msg(fd_c, MSG_TEMPO_PARTITA, tempo);

                continue;
            }
        }
        else if (richiesta -> type == MSG_POST_BACHECA) {
            char *post = richiesta -> data;

            pthread_mutex_lock(&bacheca_mtx);
            inserimento_bacheca(bacheca, player -> username, post, &n_post);
            pthread_mutex_unlock(&bacheca_mtx);

            char *msg = "Pubblicazione del messaggio avvenuta con successo";
            prepara_msg(fd_c, MSG_OK, msg);
            continue;
        }
        else if (richiesta -> type == MSG_SHOW_BACHECA) {

            pthread_mutex_lock(&bacheca_mtx);
            bacheca_strng = bacheca_a_stringa(bacheca, &n_post);
            pthread_mutex_unlock(&bacheca_mtx);
            
            prepara_msg(fd_c, MSG_SHOW_BACHECA, bacheca_strng);

            continue;
        }
        else if (richiesta -> type == MSG_REGISTRA_UTENTE) {
            char *msg = "Sei già registrato";
            prepara_msg(fd_c, MSG_ERR, msg);
            continue;
        }
        else if (richiesta -> type == MSG_PUNTI_FINALI) {
            if (fase_gioco == 0) {
                prepara_msg(fd_c, MSG_PUNTI_FINALI, classifica);
                continue;
            }
            else {
                char *msg = "Aspetta la fine del gioco per richiedere la classifica";
                prepara_msg(fd_c, MSG_ERR, msg);
                continue;
            }
        }

        free(richiesta -> data);
        free(richiesta);
    }

    free(matrice_strng);
    free(bacheca_strng);
    free(tempo);

    return NULL;
}

// rappresenta la pausa + la partita del gioco
void *gioco (void *args) {
    // attesa di giocatori

    printf("ciao sono thread gioco tid: %ld \n", pthread_self());

    while (1) {
        // se c'è almeno un giocatore, inizia una pausa iniziale
        pthread_mutex_lock(&giocatori_mtx);
        if (Giocatori -> num_giocatori == 0) {
            // finché non ci sono giocatori, c'è attesa
            printf("In attesa di giocatori... \n");

            while (Giocatori -> num_giocatori < 1) {
                pthread_cond_wait(&giocatori_cond, &giocatori_mtx);
                printf("risvegliato \n");
            }   
        }

        printf("almeno un giocatore registrato \n");
        pthread_mutex_unlock(&giocatori_mtx);
        
        // avvio del timer per la durata del gioco
        time(&tempo_pausa);
        alarm(durata_pausa * 60);

        printf("Pausa iniziata!\n");

        //sleep(durata_pausa * 60);

        pthread_mutex_lock(&fase_mtx);
        while (fase_gioco == 0) {
            pthread_cond_wait(&fase_cond, &fase_mtx);
        }
        pthread_mutex_unlock(&fase_mtx);

        // pausa in corso

        // quando finisce la pausa viene inviato un SIGALRM che imposta fase_gioco a 1

        printf("pausa finitaaaaa \n");

        printf("ora deve iniziare il gioco \n");

        // avvio del timer per la durata del gioco
        time(&tempo_gioco);
        alarm(durata_gioco * 60);

        printf("Gioco iniziato!\n");

        // capire se devo inviare la matrice casuale o la matrice inizializzata da file
        if (data_filename == NULL) {
            matrice_casuale(matrice);
        }
        else {
            inizializzazione_matrice(matrice, data_filename);
        }

        char *matrice_strng;
        SYSCN(matrice_strng, malloc(MAX_CASELLE * MAX_CASELLE * 2 + 1), "Errore nell'allocazione della stringa per la matrice");

        matrice_strng = matrice_a_stringa(matrice, matrice_strng);

        printf("sto x inviare matrice \n");

        char *msg = "Il gioco è iniziato, buona fortuna!";

        // ora che la matrice è pronta, inviarla ai giocatori insieme al tempo che sta scorrendo
        pthread_mutex_lock(&giocatori_mtx);

        giocatore *tmp = Giocatori -> head;
        while (tmp != NULL) {
            prepara_msg(tmp -> fd_c, MSG_OK, msg);
            prepara_msg(tmp -> fd_c, MSG_MATRICE, matrice_strng);
            prepara_msg(tmp -> fd_c, MSG_TEMPO_PARTITA, tempo_rimanente(tempo_gioco,durata_gioco));

            tmp = tmp -> next;
        }
        printf("matrice inviata \n");
        pthread_mutex_unlock(&giocatori_mtx);

        printf("giocando");

        sleep(durata_gioco * 60);

        // ... gioco in corso

        // quando finisce il gioco parte la alrm -> i produttori grazie all'handler vengono avviati

        printf("Gioco finito!\n");

        free(matrice_strng);
    }

    return NULL;
}

// consumatore sulla coda punteggi
// (anche consumatore sulla variabile partita_finita)
void *scorer (void *args) {

    // durante il gioco rimane in attesa con la condition variable
    // si 'risveglia' quando scatta la pausa (impostata dal gioco)
    pthread_mutex_lock(&scorer_mtx);

    while (partita_finita == 0) {
        pthread_cond_wait(&scorer_cond, &scorer_mtx);
    }

    partita_finita = 0;
    pthread_mutex_unlock(&scorer_mtx);

    printf("ciao sono lo scorer %ld \n", pthread_self());
            
    /*pthread_mutex_lock(&giocatori_mtx);
    int n = Giocatori -> num_giocatori;
    pthread_mutex_unlock(&giocatori_mtx);

    for (int i = 0; i < n; i++) {
        pthread_mutex_lock(&scorer_mtx);

        while (Punteggi == NULL) {
            pthread_cond_wait(&scorer_cond, &scorer_mtx);
        }

        // leggo dalla testa della coda
        risultato *r = leggi_risultato(Punteggi);
        if (r == NULL) {
            pthread_mutex_unlock(&scorer_mtx);
            continue;
        }

        // memorizzo temporaneamente in un array di struct (mi piace di più per il sorting)
        tmp[i].nome_utente = strdup(r-> username);
        tmp[i].punteggio = r -> punteggio;

        pthread_mutex_unlock(&scorer_mtx);
    }

    // sorting dell'array temporaneo
    qsort(tmp, n, sizeof(punteggi), sorting_classifica);

    char s[32];
    // preparare la classifica come stringa CSV
    for (int i = 0; i < n; i++) {
        snprintf(s, 32, "%s, %d \n", tmp[i].nome_utente, tmp[i].punteggio);
        strcat(classifica, s);
    }

    // mandare a tutti i thread il segnale della classifica è pronta
    pthread_mutex_lock(&client_mtx);
    invia_sigusr2(Threads, SIGUSR2);
    pthread_mutex_unlock(&client_mtx);*/

    return NULL;
}

// funzione per la gestione del server
void server(char* nome_server, int porta_server) {
    //descrittori dei socket
    int fd_client;

    int ret;

    struct sockaddr_in ind_server, ind_client;

    socklen_t len_addr;

    sigset_t set;

    printf("ciao sono il server \n");

    // creazione del socket INET restituendo il relativo file descriptor con protocollo TCP
    SYSC(fd_server, socket(AF_INET, SOCK_STREAM, 0), "Errore nella socket");

    // inizializzazione struct ind_server
    memset(&ind_server, 0, sizeof(ind_server));
    
    ind_server.sin_family = AF_INET;
    ind_server.sin_port = htons(porta_server);
    ind_server.sin_addr.s_addr = inet_addr(nome_server);

    // binding
    SYSC(ret, bind(fd_server, (struct sockaddr *)&ind_server, sizeof(ind_server)), "Errore nella bind");

    // listen
    SYSC(ret, listen(fd_server, MAX_CLIENT), "Errore nella listen");

    inizializza_lista_thread(&Threads);

    inizializza_lista_handler(&Thread_Handler);
    
    inizializza_lista_giocatori(&Giocatori);

    // creazione del thread per il gioco
    SYST(pthread_create(&gioco_tid, NULL, gioco, NULL));

    // creazione del thread per lo scorer
    SYST(pthread_create(&scorer_tid, NULL, scorer, (void*)&set));

    // ciclo di accettazione delle connessioni dei giocatori
    // server continuamente in ascolto
    while(1) {
        // accept
        len_addr = sizeof(ind_client);
        SYSC(fd_client, accept(fd_server, (struct sockaddr*)&ind_client, &len_addr), "Errore nella accept");

        // allocazione della struct per i parametri del thread
        client_args *params;
        SYSCN(params, malloc(sizeof(client_args)), "Errore nell'allocazione dei parametri del thread");

        // inizializzazione dei parametri
        params -> sck = fd_client;

        // creazione del thread
        SYST(pthread_create(&(params -> t_id), 0, thread_client, params));

        // aggiunta del thread alla lista
        pthread_mutex_lock(&client_mtx);
        inserisci_thread(Threads, params -> t_id);
        pthread_mutex_unlock(&client_mtx);
    }
}

// main per il controllo dei parametri
int main(int argc, char *ARGV[]) {

    char *nome_server;
    int porta_server;

    // struct per file
    struct stat info;

    // controllo dei parametri passati da linea di comando

    // ci devono essere almeno 3 obbligatori: nome dell'eseguibile, nome_server e numero_porta*/
    // altrimenti errore
    if (argc < 3) {
        printf("Errore! Parametri: %s nome_server porta_server \n", ARGV[0]);
        exit(EXIT_FAILURE);
    }

    nome_server = ARGV[1];
    porta_server = atoi(ARGV[2]);

    int ret, opz, op_indx = 0;

    int seed_dato = 0;

    static struct option long_options[] = {
        {"matrici", required_argument, 0, 'm'},
        {"durata", required_argument, 0, 'd'},
        {"seed", required_argument, 0, 's'},
        {"diz", required_argument, 0, 'z'},
        {0, 0, 0, 0}
    };

    while ((opz = getopt_long(argc, ARGV, "", long_options, &op_indx)) != -1) {
        switch(opz) {
            case 'm':
                data_filename = optarg;
            break;
            case 'd':
                durata_gioco = atoi(optarg);
            break;
            case 's':
                rnd_seed = atoi(optarg);
                seed_dato = 1;
            break;  
            case 'z':
                dizionario = optarg;
            break;
            default:
                printf("Errore! Parametri: %s nome_server porta_server [--matrici data_filename] [--durata durata_gioco] [--seed rnd_seed] [--diz dizionario] \n", ARGV[0]);
                exit(EXIT_FAILURE);
        }
    }

     /* controllare se sia matrici e seed sono stati forniti e dare errore */
    if (data_filename != NULL && seed_dato == 1) {
        fprintf(stderr, "Errore: Non è possibile fornire sia il parametro --matrici che il parametro --seed.\n");
        exit(EXIT_FAILURE);
    }

    /* se sono settati controllali */

    /*controllo nome del file delle matrici*/
    if (data_filename != NULL) {
        /*se è stato fornito, controllare che sia un file regolare*/
        SYSC(ret, stat(data_filename, &info), "Errore nella stat del file delle matrici! \n");
        if (!S_ISREG(info.st_mode)) {
            perror("Attenzione, file non regolare! \n");
            exit(EXIT_FAILURE);
        }
    }

    /*controllo della durata in minuti*/
    if (durata_gioco != 0) {
        /*se è stato fornito, controllare che sia un intero maggiore di 0*/
        if (durata_gioco <= 0) {
            perror("Attenzione, durata non valida!\n");
            exit(EXIT_FAILURE);
        }
    }

    /*controllo del file dizionario*/
    if (dizionario != NULL) {
        /*se è stato fornito, controllare che sia un file regolare*/
        SYSC(ret, stat(dizionario, &info), "Errore nella stat del file del dizionario! \n");
        if (!S_ISREG(info.st_mode)) {
            perror("Attenzione, file non regolare! \n");
            exit(EXIT_FAILURE);
        }
    }

    printf("parametri ok! \n");

    srand(rnd_seed);

    main_tid = pthread_self();

    printf("1 %ld \n", main_tid);

    // inizializzazione dei segnali
    inizializza_segnali();

    // creazione del trie
    radice = nuovo_nodo();

    // caricamento del dizionario all'interno del trie
    caricamento_dizionario(dizionario, radice);

    // allocazione della matrice
    matrice = allocazione_matrice();

    matrice_casuale(matrice);

    // allocazione della bacheca
    bacheca = allocazione_bacheca();

    printf("2 \n");

    // inizializzazione mutex
    SYST(pthread_mutex_init(&client_mtx, NULL));
    SYST(pthread_mutex_init(&handler_mtx, NULL));

    SYST(pthread_mutex_init(&giocatori_mtx, NULL));
    SYST(pthread_cond_init(&giocatori_cond, NULL));

    SYST(pthread_mutex_init(&parole_mtx, NULL));

    SYST(pthread_mutex_init(&scorer_mtx, NULL));
    SYST(pthread_cond_init(&scorer_cond, NULL));

    SYST(pthread_mutex_init(&bacheca_mtx, NULL));
    SYST(pthread_mutex_init(&fase_mtx, NULL));

    server(nome_server, porta_server);

    return 0;
}