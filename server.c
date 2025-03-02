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

// tid
pthread_t main_tid;
pthread_t scorer_tid;
pthread_t gioco_tid;
pthread_t chiusura_tid;

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

// variabile globale per la chiusura del server
int chiudi = 0;

// variabile globale per memorizzare quanti hanno inviato il messaggio di chiusura al proprio client
int num_chiusure = 0;

// strutture dati globali

// trie
Trie *radice = NULL;

// bacheca
Bacheca *bacheca;

// matrice come array di stringhe
char **matrice;

// lista dei thread
lista_thread *Threads;

// lista dei giocatori
lista_giocatori *Giocatori;

// coda dei punteggi
coda_risultati *Punteggi;

// classifica
char classifica[MAX_DIM];

// mutex
pthread_mutex_t client_mtx;
pthread_mutex_t giocatori_mtx;
pthread_mutex_t parole_mtx;
pthread_mutex_t scorer_mtx;
pthread_mutex_t coda_mtx;
pthread_mutex_t bacheca_mtx;
pthread_mutex_t fase_mtx;
pthread_mutex_t sig_mtx;
pthread_mutex_t chiusura_mtx;

// condition variable
pthread_cond_t giocatori_cond;
pthread_cond_t scorer_cond;
pthread_cond_t coda_cond;
pthread_cond_t fase_cond;
pthread_cond_t sig_cond;
pthread_cond_t chiusura_cond;

// gestione dei segnali

// inizializzazione dei segnali centralizzata
void inizializza_segnali() {
    int ret;

    struct sigaction sa_signals, sa_sigint, sa_sigalrm;
    sigset_t set;

    // inizializzazione della maschera
    sigemptyset(&set);

    // aggiunta di SIGURS1 e SIGUSR2 alla maschera
    SYSC(ret, sigaddset(&set, SIGUSR1), "Errore nell'aggiunta di SIGUSR1 alla maschera");
    SYSC(ret, sigaddset(&set, SIGUSR2), "Errore nell'aggiunta di SIGUSR2 alla maschera");

    // blocco dei segnali SIGUSR1 e SIGUSR2 a livello di processo -> vengono poi sbloccati nei singoli thread client
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    sa_signals.sa_handler = sigclient_handler; // viene gestito dall'handler client
    sa_signals.sa_mask = set;
    sa_signals.sa_flags = SA_RESTART;

    // associazione del set con handler, maschera e flag ai segnali SIGUSR1 e SIGUSR2
    SYST(sigaction(SIGUSR1, &sa_signals, NULL));
    SYST(sigaction(SIGUSR2, &sa_signals, NULL));

    // 'pulizia' della maschera per aggiungere SIGINT
    sigemptyset(&set);

    // aggiunta di SIGINT alla maschera
    SYSC(ret, sigaddset(&set, SIGINT), "Errore nell'aggiunta di SIGINT alla maschera");

    sa_sigint.sa_handler = sigint_handler; // assegnazione dell'handler
    sa_sigint.sa_mask = set;
    sa_sigint.sa_flags = 0; // 0 per non far riavviare le system calls interrotte

    // configurazione del set per SIGINT
    SYST(sigaction(SIGINT, &sa_sigint, NULL));

    // pulizia della maschera per configurare SIGALRM
    sigemptyset(&set);

    // aggiunta di SIGALRM alla maschera
    SYSC(ret, sigaddset(&set, SIGALRM), "Errore nell'aggiunta di SIGALRM alla maschera");

    // blocco di SIGALRM per processo -> viene sbloccato nel thread di gioco
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    sa_sigalrm.sa_handler = sigalrm_handler; // assegnazione dell'handler
    sa_sigalrm.sa_mask = set;
    sa_sigalrm.sa_flags = SA_RESTART;      // per riavviare le chiamate interrotte

    // configurazione del set per SIGALRM
    SYST(sigaction(SIGALRM, &sa_sigalrm, NULL));
}

void *chiusura (void *args) {
    // il thread si risveglia solo quando il processo riceve SIGINT
    pthread_mutex_lock(&sig_mtx);
    while (chiudi == 0) {
        pthread_cond_wait(&sig_cond, &sig_mtx);
    }
    pthread_mutex_unlock(&sig_mtx);

    if (Threads -> num_thread > 0) {
        // attesa che tutti i thread abbiano inviato il messaggio al proprio client
        pthread_mutex_lock(&chiusura_mtx);
        while (num_chiusure < Threads -> num_thread) {
            pthread_cond_wait(&chiusura_cond, &chiusura_mtx);
        }
        pthread_mutex_unlock(&chiusura_mtx);

        // inviati i messaggi, chiudere i file descriptor e cancellare i thread relativi
        pthread_mutex_lock(&client_mtx);
        thread_attivo *tmp = Threads -> head;
        while (tmp != NULL) {
            close(tmp -> fd_c);
            SYST(pthread_cancel(tmp -> t_id));
            tmp = tmp -> next;
        }
    }

    // eliminare la lista
    svuota_lista_thread(Threads);
    free(Threads);

    pthread_exit(NULL);
}

// handler per il sigint
void sigint_handler (int sig) {
    int ret;

    // setto la flag per la chiusura
    pthread_mutex_lock(&sig_mtx);
    chiudi = 1;
    pthread_cond_signal(&sig_cond);
    pthread_mutex_unlock(&sig_mtx);

    // chiusura del gioco
    SYST(pthread_cancel(gioco_tid));

    // chiusura dello scorer
    SYST(pthread_cancel(scorer_tid));

    // eliminare la lista delle parole associata ad ogni giocatore
    if (Giocatori -> num_giocatori > 0) {
        pthread_mutex_lock(&giocatori_mtx);
        giocatore *tmp_g = Giocatori -> head;
        while (tmp_g != NULL) {
            if (tmp_g -> parole_trovate) {
                svuota_lista_parole(tmp_g -> parole_trovate);
                free(tmp_g -> parole_trovate);
            }
            tmp_g = tmp_g -> next;
        }
        pthread_mutex_unlock(&giocatori_mtx);

        free(tmp_g);
    }
    
    svuota_lista_giocatori(Giocatori);
    free(Giocatori);

    // liberare la coda punteggi se era stata riempita
    if (Punteggi -> num_risultati > 0) {
        svuota_coda_risultati(Punteggi);
        free(Punteggi);
    }

    // deallocazione di mutex e condition variable
    /*SYST(pthread_mutex_destroy(&client_mtx));
    SYST(pthread_mutex_destroy(&giocatori_mtx));
    SYST(pthread_mutex_destroy(&parole_mtx));
    SYST(pthread_mutex_destroy(&scorer_mtx));
    SYST(pthread_mutex_destroy(&coda_mtx));
    SYST(pthread_mutex_destroy(&bacheca_mtx));
    SYST(pthread_mutex_destroy(&fase_mtx));
    SYST(pthread_mutex_destroy(&sig_mtx));
    SYST(pthread_mutex_destroy(&chiusura_mtx));

    SYST(pthread_cond_destroy(&giocatori_cond));
    SYST(pthread_cond_destroy(&scorer_cond));
    SYST(pthread_cond_destroy(&coda_cond));
    SYST(pthread_cond_destroy(&fase_cond));
    SYST(pthread_cond_destroy(&sig_cond));
    SYST(pthread_cond_destroy(&chiusura_cond));*/

    // deallocazione bacheca
    if (bacheca) {
        deallocazione_bacheca(bacheca);
    }

    // deallocazione matrice
    if (matrice) {
        deallocazione_matrice(matrice);
    }

    // deallocazione trie
    if (radice) {
        deallocazione_trie(radice);
    }

    // chiudere il socket lato server
    SYSC(ret, close(fd_server), "Errore nella chiusura del socket");

    exit(EXIT_SUCCESS);
}

// handler per la gestione dei segnale per i client 
// produttori sulla variabile per le chiusure e sulla coda dei punteggi
void sigclient_handler (int sig) {
    if (sig == SIGUSR1) {
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
        pthread_mutex_lock(&coda_mtx);

        // inserimento del punteggio nella coda
        inserisci_risultato(Punteggi, username, punteggio);

        Punteggi -> num_risultati++;

        // lo scorer viene avvisato che è stato scritto un punteggio sulla coda
        pthread_cond_signal(&coda_cond);

        // rilascio della mutex
        pthread_mutex_unlock(&coda_mtx);
    }
    else if (sig == SIGUSR2) {
        if (chiudi == 0) {

            pthread_mutex_lock(&giocatori_mtx);
            int fd_c = recupera_fd_giocatore(Giocatori, pthread_self());
            pthread_mutex_unlock(&giocatori_mtx);

            // invio del messaggio con la classifica in CSV
            prepara_msg(fd_c, MSG_PUNTI_FINALI, classifica);
        }
        else {
            // riutilizzo di SIGUSR2 per inviare il messaggio di chiusura
            pthread_mutex_lock(&client_mtx);
            int fd_c = recupera_fd_thread(Threads, pthread_self());
            pthread_mutex_unlock(&client_mtx);

            char *msg = "Il server sta chiudendo, disconnessione in corso";

            // invio del messaggio di chiusura al proprio client
            prepara_msg(fd_c, MSG_SERVER_SHUTDOWN, msg);

            pthread_mutex_lock(&chiusura_mtx);

            // incremento del contatore delle chiusure
            num_chiusure++;
            
            // segnale per togliere dall'attesa il thread di chiusura
            pthread_cond_signal(&chiusura_cond);
            pthread_mutex_unlock(&chiusura_mtx);
        }
    }
}

// segnale per gestione dei tempi e delle fasi del gioco
void sigalrm_handler (int sig) {
    // se è finita la partita, mettere il gioco in pausa
    // mandare il segnale SIGUSR1 a sigclient handler e allo scorer
    pthread_mutex_lock(&fase_mtx);
    if (fase_gioco == 1) {
        fase_gioco = 0;
        pthread_mutex_unlock(&fase_mtx);

        // avviso allo scorer per farlo risvegliare
        pthread_mutex_lock(&scorer_mtx);
        partita_finita = 1;
        pthread_cond_signal(&scorer_cond);
        pthread_mutex_unlock(&scorer_mtx);

        // invio del segnale ai thread
        if (Threads -> num_thread > 0) {
            pthread_mutex_lock(&client_mtx);
            invia_sigusr(Threads, SIGUSR1);
            pthread_mutex_unlock(&client_mtx);
        }

        printf("Segnale inviato \n");
    }
    else {
        // se è finita la pausa, rimettere il gioco in corso
        fase_gioco = 1;
        pthread_cond_signal(&fase_cond);
        pthread_mutex_unlock(&fase_mtx);
    }
}

// funzioni di utilità

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
    time_t trascorso = tempo_attuale - tempo;

    // conversione in secondi
    time_t in_secondi = (time_t)minuti * 60;

    // calcolo del tempo rimanente
    time_t rimanente = in_secondi - trascorso;

    // allocazione della stringa per restituire il tempo rimanente
    char *t;
    SYSCN(t, (char*)malloc(50 * sizeof(char)), "Errore nell'allocazione della stringa per il tempo rimanente");

    sprintf(t, "%ld", (long)rimanente);

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

    sigset_t set_client;
    sigemptyset(&set_client);

    // alla maschera si aggiungono i segnali personalizzati
    sigaddset(&set_client, SIGUSR1);
    sigaddset(&set_client, SIGUSR2);

    // sbloccare i segnali della maschera per thread_client
    SYST(pthread_sigmask(SIG_UNBLOCK, &set_client, NULL));

    // allo stesso modo si blocca il sigint in modo che venga gestito solo dal main thread
    sigset_t set_client_int;
    sigemptyset(&set_client_int);
    sigaddset(&set_client_int, SIGINT);
    SYST(pthread_sigmask(SIG_BLOCK, &set_client_int, NULL));

    // giocatore associato al thread
    giocatore *player = NULL;

    lista_parole *Parole_Trovate = NULL;

    // variabile per ricevere il messaggio
    Msg_Socket *richiesta = NULL;

    char *tempo = NULL;

    // allocazione della stringa per la matrice
    char *matrice_strng;
    size_t dim = MAX_CASELLE * MAX_CASELLE * 2;
    SYSCN(matrice_strng, malloc(dim * sizeof(char)), "Errore nell'allocazione della stringa per la matrice");

    char *bacheca_strng = NULL;

    // tutto il processo di gestione del client viene eseguito fino a quando non si chiude il client stesso
    
    while (1) {
        // prima della registrazione, si accettano solo messaggi di registrazione e di fine
        // (messaggio di aiuto viene gestito nel client)
        richiesta = ricezione_msg(fd_c);

        // chiusura del client senza scrivere fine
        if (richiesta == NULL) {
            // eliminare il thread dalla lista dei thread
            pthread_mutex_lock(&client_mtx);
            rimuovi_thread(Threads, pthread_self());
            Threads -> num_thread--;
            pthread_mutex_unlock(&client_mtx);

            // deallocare la struct dei suoi parametri
            free(params);

            // dealloca tutta la memoria allocata fino ad ora
            free(matrice_strng);
            free(Parole_Trovate);

            // terminazione del thread
            pthread_exit(NULL);
        }

        /*printf("tipo messaggio: %c \n", richiesta -> type);
        printf("messaggio ricevuto: %s \n", richiesta -> data);*/

        if (richiesta -> type == MSG_ERR) {
            // se il client invia "fine"
            pthread_mutex_lock(&client_mtx);
            rimuovi_thread(Threads, pthread_self());
            Threads -> num_thread--;
            pthread_mutex_unlock(&client_mtx);
            
            free(params);
            free(matrice_strng);
            free(Parole_Trovate);
            
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

            Giocatori -> num_giocatori++;

            // signal per avvisare il gioco di partire
            pthread_cond_signal(&giocatori_cond);
            pthread_mutex_unlock(&giocatori_mtx);

            printf("Giocatore %s del thread %ld registrato \n", nome_utente, pthread_self());

            // inviare messaggio di avvenuta registrazione
            char *msg = "Registrazione avvenuta con successo, sei pronto a giocare?";
            prepara_msg(fd_c, MSG_OK, msg);
            break;
        }

        // messaggio non valido
        char *msg = "Messaggio non valido";
        prepara_msg(fd_c, MSG_ERR, msg);

        // pulire le variabili appena utilizzate per lo scambio di messaggi di questa fase
        if (richiesta -> data) {
            free(richiesta -> data);
        }
        free(richiesta);
    }

    if (richiesta -> data) {
        free(richiesta -> data);
    }
    free(richiesta);

    // invio della matrice e del tempo
    if (fase_gioco == 0) {
        // gioco in pausa -> invio del tempo di attesa
        tempo = tempo_rimanente(tempo_pausa, durata_pausa);

        prepara_msg(fd_c, MSG_TEMPO_ATTESA, tempo);

        free(tempo);
    }
    else {
        // gioco in corso -> invio matrice e tempo rimanente
        matrice_strng = matrice_a_stringa(matrice, matrice_strng);

        prepara_msg(fd_c, MSG_MATRICE, matrice_strng);

        tempo = tempo_rimanente(tempo_gioco, durata_gioco);

        prepara_msg(fd_c, MSG_TEMPO_PARTITA, tempo); 

        free(tempo); 
    }

    // ciclo di ascolto durante il gioco
    while (1) {

        richiesta = ricezione_msg(fd_c);

        // chiusura senza scrittura di fine
        if (richiesta == NULL) {
            // eliminare lista parole trovate
            pthread_mutex_lock(&parole_mtx);
            svuota_lista_parole(player -> parole_trovate);
            pthread_mutex_unlock(&parole_mtx);

            // eliminare il giocatore dalla lista dei giocatori
            pthread_mutex_lock(&giocatori_mtx);
            rimuovi_giocatore(Giocatori, player -> username);
            Giocatori -> num_giocatori--;
            pthread_mutex_unlock(&giocatori_mtx);

            pthread_mutex_lock(&client_mtx);
            rimuovi_thread(Threads, pthread_self());
            Threads -> num_thread--;
            pthread_mutex_unlock(&client_mtx);

            // deallocare la struct dei suoi parametri
            free(params);

            free(matrice_strng);
            free(bacheca_strng);
            free(richiesta);

            // terminazione del thread
            pthread_exit(NULL);
        }

        if (richiesta -> type == MSG_ERR) {
            // se il client invia "fine"
            // eliminare il thread dalla lista dei thread
            printf("Chiusura di questo client %ld \n", pthread_self());

            // eliminare lista parole trovate
            pthread_mutex_lock(&parole_mtx);
            svuota_lista_parole(player -> parole_trovate);
            pthread_mutex_unlock(&parole_mtx);

            // eliminare il giocatore dalla lista dei giocatori
            pthread_mutex_lock(&giocatori_mtx);
            rimuovi_giocatore(Giocatori, player -> username);
            Giocatori -> num_giocatori--;
            pthread_mutex_unlock(&giocatori_mtx);

            pthread_mutex_lock(&client_mtx);
            rimuovi_thread(Threads, pthread_self());
            Threads -> num_thread--;
            pthread_mutex_unlock(&client_mtx);

            // deallocare la struct dei suoi parametri
            free(params);

            free(matrice_strng);
            free(bacheca_strng);
            free(richiesta);
            
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
                char *par;
                SYSCN(par, (char*)malloc((len + 1)), "Errore nell'allocazione della parola");
                int i = 0, j = 0;

                // prima di ricercare nella matrice togliere eventuali occorrenze di "qu"
                if (strstr(p, "qu")) {
                    while (i < len) {
                        if ((i + 1) < len && p[i] == 'q' && p[i + 1] == 'u') {
                            par[j] = 'q';
                            i += 2;
                        }
                        else {
                            par[j] = p[i];
                            i++;
                        }
                        j++;
                    }
                    par[j] = '\0';
                }
                else {
                    // se 'qu' non è presente, si copia semplicemente la parola
                    strcpy(par, p);
                }

                strcpy(p, par);

                if (ricerca_matrice(matrice, p) == 0) {
                    char *msg = "Parola non presente nella matrice";
                    prepara_msg(fd_c, MSG_ERR, msg);

                    free(par);

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

                free(par);

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

                free(tempo);
                continue;
            }
            else {
                matrice_strng = matrice_a_stringa(matrice, matrice_strng);

                prepara_msg(fd_c, MSG_MATRICE, matrice_strng);

                tempo = tempo_rimanente(tempo_gioco, durata_gioco);

                prepara_msg(fd_c, MSG_TEMPO_PARTITA, tempo);

                free(tempo);

                continue;
            }
        }
        else if (richiesta -> type == MSG_POST_BACHECA) {
            char *post = richiesta -> data;

            pthread_mutex_lock(&bacheca_mtx);
            inserimento_bacheca(bacheca, player -> username, post);
            pthread_mutex_unlock(&bacheca_mtx);

            char *msg = "Pubblicazione del messaggio avvenuta con successo";
            prepara_msg(fd_c, MSG_OK, msg);
            continue;
        }
        else if (richiesta -> type == MSG_SHOW_BACHECA) {

            pthread_mutex_lock(&bacheca_mtx);
            bacheca_strng = bacheca_a_stringa(bacheca);
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

        // messaggio non valido
        char *msg = "Messaggio non valido";
        prepara_msg(fd_c, MSG_ERR, msg);

        // appena sono in pausa svuoto la lista di parole
        while (fase_gioco != 1 && player -> parole_trovate -> num_parole > 0) {
            pthread_mutex_lock(&parole_mtx);
            svuota_lista_parole(player -> parole_trovate);
            pthread_mutex_unlock(&parole_mtx);
        }

        if (richiesta -> data) {
            free(richiesta -> data);
        }
        free(richiesta);
    }

    return NULL;
}

// rappresenta la pausa + la partita del gioco
void *gioco (void *args) {
    sigset_t set;

    // segnale SIGALRM viene sbloccato per il gioco
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);

    while (1) {
        // se c'è almeno un giocatore, inizia una pausa iniziale
        pthread_mutex_lock(&giocatori_mtx);
        if (Giocatori -> num_giocatori == 0) {
            // finché non ci sono giocatori, c'è attesa
            printf("In attesa di giocatori... \n");

            while (Giocatori -> num_giocatori < 1) {
                pthread_cond_wait(&giocatori_cond, &giocatori_mtx);
            }   
        }

        pthread_mutex_unlock(&giocatori_mtx);
        
        // avvio del timer per la durata del gioco
        time(&tempo_pausa);
        alarm(durata_pausa * 60);

        printf("Pausa iniziata! \n");

        // sleep(durata_pausa * 60);

        pthread_mutex_lock(&fase_mtx);
        while (fase_gioco == 0) {
            pthread_cond_wait(&fase_cond, &fase_mtx);
        }
        pthread_mutex_unlock(&fase_mtx);

        // pausa in corso

        // quando finisce la pausa viene inviato un SIGALRM che imposta fase_gioco a 1

        printf("Pausa finita! \n");

        printf("Ora deve iniziare il gioco \n");

        // avvio del timer per la durata del gioco
        time(&tempo_gioco);
        alarm(durata_gioco * 60);

        printf("Gioco iniziato! \n");

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

        char *msg = "Il gioco è iniziato, buona fortuna!";

        // ora che la matrice è pronta, inviarla ai giocatori insieme al tempo che sta scorrendo
        pthread_mutex_lock(&giocatori_mtx);

        giocatore *tmp = Giocatori -> head;
        while (tmp != NULL) {
            char *tempo = NULL;

            prepara_msg(tmp -> fd_c, MSG_OK, msg);
            prepara_msg(tmp -> fd_c, MSG_MATRICE, matrice_strng);
            tempo = tempo_rimanente(tempo_gioco, durata_gioco);
            prepara_msg(tmp -> fd_c, MSG_TEMPO_PARTITA, tempo);

            tmp = tmp -> next;

            free(tempo);
        }
        printf("Matrice inviata \n");
        pthread_mutex_unlock(&giocatori_mtx);

        sleep(durata_gioco * 60);

        // ... gioco in corso

        // quando finisce il gioco parte la alrm -> i thread client diventano produttori sulla coda punteggi

        printf("Gioco finito! \n");

        free(matrice_strng);
    }

    return NULL;
}

// consumatore sulla variabile partita_finita e sulla coda punteggi
void *scorer (void *args) {
    while (1) {
        // durante il gioco rimane in attesa con la condition variable
        // si 'risveglia' quando scatta la pausa
        pthread_mutex_lock(&scorer_mtx);

        while (partita_finita == 0) {
            pthread_cond_wait(&scorer_cond, &scorer_mtx);
        }

        // reimpostare flag a 0
        partita_finita = 0;
        pthread_mutex_unlock(&scorer_mtx);

        // pulizia della classifica
        memset(classifica, 0, sizeof(classifica));

        // recuperare il numero di giocatori
        pthread_mutex_lock(&giocatori_mtx);
        int n = Giocatori -> num_giocatori;
        pthread_mutex_unlock(&giocatori_mtx);

        pthread_mutex_lock(&coda_mtx);

        // ora attesa sulla coda dei punteggi fino a quando tutti inseriscono il proprio risultato
        while (Punteggi -> num_risultati < n) {
            pthread_cond_wait(&coda_cond, &coda_mtx);
        }

        // array per memorizzare temporaneamente i dati della coda
        punteggi tmp[n];
        memset(tmp, 0, sizeof(tmp));

        int i = 0;

        // scorrere la coda per recuperare i punteggi
        risultato *r = Punteggi -> head;
        while (r != NULL && i < n) {

            size_t len = strlen(r->username) + 1;

            SYSCN(tmp[i].nome_utente, malloc(len), "Errore nell'allocazione del nome utente");
            
            // memorizzare temporaneamente in un array di struct
            strcpy(tmp[i].nome_utente, r -> username);
            tmp[i].punteggio = r -> punteggio;

            i++;
            r = r -> next;
        }

        stampa_coda_risultati(Punteggi);

        Punteggi -> num_risultati = 0;

        pthread_mutex_unlock(&coda_mtx);

        // sorting dell'array temporaneo
        qsort(tmp, i, sizeof(punteggi), sorting_classifica);

        classifica[0] = '\0';
        // preparare la classifica come stringa CSV
        for (int j = 0; j < i; j++) {
            char s[64];
            snprintf(s, sizeof(s), "%s: %d\n", tmp[j].nome_utente, tmp[j].punteggio);
            strncat(classifica, s, sizeof(classifica) - strlen(classifica) - 1);
            free(tmp[j].nome_utente);
        }

        printf("Classifica in csv: \n %s", classifica);

        svuota_coda_risultati(Punteggi);

        // mandare a tutti i thread il segnale della classifica è pronta
        if (Threads -> num_thread > 0) {
            pthread_mutex_lock(&client_mtx);
            invia_sigusr(Threads, SIGUSR2);
            pthread_mutex_unlock(&client_mtx);
        }

    }

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
    
    inizializza_lista_giocatori(&Giocatori);

    inizializza_coda_risultati(&Punteggi);

    // creazione del thread di chiusura
    SYST(pthread_create(&chiusura_tid, NULL, chiusura, NULL));

    // creazione del thread per il gioco
    SYST(pthread_create(&gioco_tid, NULL, gioco, NULL));

    // creazione del thread per lo scorer
    SYST(pthread_create(&scorer_tid, NULL, scorer, (void*)&set));

    // ciclo di accettazione delle connessioni dei giocatori -> server continuamente in ascolto
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
        inserisci_thread(Threads, params -> t_id, fd_client);
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

    // controllare se sia matrici e seed sono stati forniti e dare errore
    if (data_filename != NULL && seed_dato == 1) {
        fprintf(stderr, "Errore: Non è possibile fornire sia il parametro --matrici che il parametro --seed.\n");
        exit(EXIT_FAILURE);
    }

    // se sono settati vanno controllati

    // controllo file delle matrici
    if (data_filename != NULL) {
        // se è stato fornito, controllare che sia un file regolare
        SYSC(ret, stat(data_filename, &info), "Errore nella stat del file delle matrici! \n");
        if (!S_ISREG(info.st_mode)) {
            perror("Attenzione, file non regolare! \n");
            exit(EXIT_FAILURE);
        }
    }

    //controllo della durata in minuti
    if (durata_gioco != 0) {
        // se è stato fornito, controllare che sia un intero maggiore di 0
        if (durata_gioco <= 0) {
            perror("Attenzione, durata non valida!\n");
            exit(EXIT_FAILURE);
        }
    }

    // controllo del file dizionario
    if (dizionario != NULL) {
        // se è stato fornito, controllare che sia un file regolare
        SYSC(ret, stat(dizionario, &info), "Errore nella stat del file del dizionario! \n");
        if (!S_ISREG(info.st_mode)) {
            perror("Attenzione, file non regolare! \n");
            exit(EXIT_FAILURE);
        }
    }

    srand(rnd_seed);

    main_tid = pthread_self();

    // inizializzazione dei segnali
    inizializza_segnali();

    // creazione del trie
    radice = nuovo_nodo();

    // caricamento del dizionario all'interno del trie
    caricamento_dizionario(dizionario, radice);

    // allocazione della matrice
    matrice = allocazione_matrice();

    // allocazione della bacheca
    bacheca = allocazione_bacheca();

    // inizializzazione mutex
    SYST(pthread_mutex_init(&client_mtx, NULL));

    SYST(pthread_mutex_init(&giocatori_mtx, NULL));
    SYST(pthread_cond_init(&giocatori_cond, NULL));

    SYST(pthread_mutex_init(&parole_mtx, NULL));

    SYST(pthread_mutex_init(&scorer_mtx, NULL));
    SYST(pthread_cond_init(&scorer_cond, NULL));

    SYST(pthread_mutex_init(&coda_mtx, NULL));
    SYST(pthread_cond_init(&coda_cond, NULL));

    SYST(pthread_mutex_init(&bacheca_mtx, NULL));

    SYST(pthread_mutex_init(&fase_mtx, NULL));
    SYST(pthread_cond_init(&fase_cond, NULL));

    SYST(pthread_mutex_init(&sig_mtx, NULL));
    SYST(pthread_cond_init(&sig_cond, NULL));

    SYST(pthread_mutex_init(&chiusura_mtx, NULL));
    SYST(pthread_cond_init(&chiusura_cond, NULL));

    server(nome_server, porta_server);

    return 0;
}