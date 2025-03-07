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
#include <ctype.h>
#include <signal.h>

#include <sys/socket.h> 
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "header/client.h"
#include "header/macros.h"
#include "header/shared.h"
#include "header/bacheca.h"
#include "header/matrice.h"

// variabili globali

// file descriptor socket lato client
int fd_client;

thread_args *comunicazione;

char *PAROLIERE = "[PROMPT PAROLIERE] --> ";

// funzioni utili

void stampa_comandi() {
    printf("Comandi disponibili: \n"
            "\t aiuto - richiedere i comandi disponibili \n"
            "\t registra_utente <nome_utente> - registrazione al gioco \n"
            "\t matrice - richiedere la matrice corrente e il tempo\n"
            "\t msg <testo> - postare un messaggio sulla bacheca \n"
            "\t show_msg - stampa della bacheca \n"
            "\t p <parola> - proporre una parola \n"
            "\t classifica - richiedere la classifica \n"
            "\t fine - uscire dal gioco \n");
    fflush(stdout);
}

void comando_non_valido() {
    printf("Richiesta non valida! \n"
            "Per visualizzare i comandi, digitare 'aiuto' \n");
    fflush(stdout);
}

int controllo_lunghezza_max (char *argomento, int max_lunghezza) {
    if (strlen(argomento) > max_lunghezza) {
        return 0;
    }
    return 1;
}

int controllo_lunghezza_min (char *argomento, int min_lunghezza) {
    if (strlen(argomento) < min_lunghezza) {
        return 0;
    }
    return 1;
}

int username_valido(char *nome_utente) {
    while (*nome_utente) {
        if (!isdigit(*nome_utente) && !islower(*nome_utente)) {
            return 0;
        }
        nome_utente++;
    }
    return 1;
}

// funzione per l'inizializzazione dei segnali che il client deve gestire
void inizializza_segnali () {
    struct sigaction sa_int, sa_usr1, sa_usr2;
    sigset_t set;
    
    // bloccare SIGUSR1 e SIGUSR2 nel thread principale
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGUSR2);
    SYST(pthread_sigmask(SIG_BLOCK, &set, NULL));

    // handler per sigint
    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0; // 0 dato che sono segnali per la chiusura, non si vuole riavviare
    SYST(sigaction(SIGINT, &sa_int, NULL));

    // handler per sigusr1
    sa_usr1.sa_handler = invio_handler;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = 0;
    SYST(sigaction(SIGUSR1, &sa_usr1, NULL));

    // handler per sigusr2
    sa_usr2.sa_handler = ricezione_handler;
    sigemptyset(&sa_usr2.sa_mask);
    sa_usr2.sa_flags = 0;
    SYST(sigaction(SIGUSR2, &sa_usr2, NULL));
}

// handler per gestire il SIGINT per la chiusura del client
void sigint_handler (int sig) {
    int ret;

    // invio del messaggio di chiusura del client al server
    prepara_msg(fd_client, MSG_ERR, NULL);

    // eliminare thread invio
    SYST(pthread_cancel(comunicazione[0].t_id));

    // eliminare thread ricezione
    SYST(pthread_cancel(comunicazione[1].t_id));

    // liberare struct comunicazione dei thread
    if (comunicazione != NULL) {
        free(comunicazione);
    }

    // chiusura del socket
    SYSC(ret, close(fd_client), "Errore nella chiusura del socket");
}

// handler per SIGUSR1
void invio_handler (int sig) {
    // terminazione
    pthread_exit(NULL);
    return;
}

// handler per SIGUSR2
void ricezione_handler (int sig) {
    // terminazione
    pthread_exit(NULL);
    return;
}

// thread per leggere comandi da tastiera e inviarli al server
void *invio_client (void *args) {
    int ret;

    // dagli argomenti recuperare il descrittore del socket
    thread_args *param = (thread_args *)args;
    int fd_c = param->sck;

    // sbloccare SIGUSR1
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    SYST(pthread_sigmask(SIG_UNBLOCK, &set, NULL));

    while (1) {
        // lettura da standard input
        char buffer[MAX_LUNGHEZZA_STDIN];
        ssize_t n_read;

        SYSC(n_read, read(STDIN_FILENO, buffer, MAX_LUNGHEZZA_STDIN - 1), "Errore nella lettura da STDIN");
        buffer[n_read] = '\0';
        // rimozione del carattere \n
        buffer[strcspn(buffer, "\n")] = '\0';

        // variabile per memorizzare il comando richiesto, grande quanto i byte effettivamente letti 
        char *msg_stdin;
        SYSCN(msg_stdin, (char*)malloc(n_read + 1), "Errore nella malloc");

        // copia di ciò che è stato letto nel buffer nella variabile del messaggio
        strncpy(msg_stdin, buffer, n_read);
        // inserimento del terminatore di stringa in ultima posizione
        msg_stdin[n_read] = '\0';

        // tokenizzare il contenuto di msg_stdin per prendere il comando
        char *comando = strtok(msg_stdin, " ");

        // controllo per prevenire situazioni dove il comando è NULL (ad esempio se viene premuto invio senza scrivere nulla)
        if (comando == NULL) {
            comando_non_valido();
            free(msg_stdin);
            continue;
        }

        // tokenizzare di nuovo per prendere eventuali parametri
        char *argomento = strtok(NULL, " ");

        // if con tutte le casistiche di messaggio
        if (strcmp(comando, "aiuto") == 0 && argomento == NULL) {
            stampa_comandi();
            free(msg_stdin);
            continue;
        }
        // registrazione
        else if (strcmp(comando, "registra_utente") == 0) {
            // prima di inviare il nome utente al server per la registrazione, controllarne la validità
            if (argomento == NULL) {
                // se il parametro non viene inserito, stampare messaggio di errore per dare la possibilità di inviare un altro comando
                comando_non_valido();
                free(msg_stdin);
                continue;
            }
            // controllo sulla lunghezza compresa tra 3 e 10 caratteri
            else if (!(controllo_lunghezza_min(argomento, MIN_LUNGHEZZA_USERNAME))) {
                printf("Nome utente troppo corto! \n"
                        "Deve avere almeno %d caratteri \n", MIN_LUNGHEZZA_USERNAME);
            }
            else if (!(controllo_lunghezza_max(argomento, MAX_LUNGHEZZA_USERNAME))) {
                printf("Nome utente troppo lungo! \n"
                        "Deve avere massimo %d caratteri \n", MAX_LUNGHEZZA_USERNAME);
            }
            // controllo sulla presenza di caratteri alfanumerici e tutti minuscoli
            else if (!(username_valido(argomento))) {
                printf("Nome utente non valido! \n"
                        "Deve essere tutto minuscolo e contenere solo caratteri alfanumerici \n");
            }
            else {
                // se il nome utente è valido invio del messaggio al server per la registrazione
                prepara_msg(fd_c, MSG_REGISTRA_UTENTE, argomento);

                free(msg_stdin);
            }
        }
        // richiesta della matrice
        else if (strcmp(comando, "matrice") == 0 && argomento == NULL) {
            prepara_msg(fd_c, MSG_MATRICE, NULL);

            free(msg_stdin);
        }
        // pubblicazione del messaggio sulla bacheca
        else if (strcmp(comando, "msg") == 0) {
            // se il messaggio non c'è il comando non è valido
            if (argomento == NULL) {
                comando_non_valido();
                continue;
            }

            // iterare con strtok per prendere tutte le stringhe del messaggio
            char *msg_intero = strtok(NULL, "");
            if (msg_intero != NULL) {
                size_t len = strlen(argomento) + strlen(msg_intero) + 2; // spazio per concatenazione e terminatore
                // stringa temporanea per concatenare tutte le stringhe
                char *full_argomento;
                SYSCN(full_argomento, (char *)malloc(len), "Errore nell'allocazione della memoria");
                
                snprintf(full_argomento, len, "%s %s", argomento, msg_intero);
                argomento = full_argomento; // usare la stringa concatenata come nuovo argomento
            }

            if (strlen(argomento) == 0) {
                printf("Messaggio vuoto!\n");
            }
            // controllo sulla lunghezza
            else if (!(controllo_lunghezza_max(argomento, MAX_CARATTERI_MESSAGGIO))) {
                printf("Messaggio troppo lungo!\n"
                        "Massimo %d caratteri \n", MAX_CARATTERI_MESSAGGIO);
            }
            else {
                // se il messaggio è valido inoltrare al server per pubblicarlo
                prepara_msg(fd_c, MSG_POST_BACHECA, argomento);

                free(msg_stdin);
            }
        }
        // richiesta di visualizzazione della bacheca
        else if (strcmp(comando, "show_msg") == 0 && argomento == NULL) {
            prepara_msg(fd_c, MSG_SHOW_BACHECA, NULL);

            free(msg_stdin);
        }
        // proposta della parola
        else if (strcmp(comando, "p") == 0) {
            if (argomento == NULL) {
                comando_non_valido();
                free(msg_stdin);
                continue;
            }
            // se c'è più di una parola dopo p stampare messaggio di errore
            else if (strtok(NULL, " ") != NULL) {
                printf("Errore: il comando 'p' accetta un solo argomento! \n");
                free(msg_stdin);
                continue;
            }
            // controllo sulla lunghezza minima
            else if (!(controllo_lunghezza_min(argomento, MIN_LUNGHEZZA_PAROLA))) {
                // se la parola ha meno di 4 caratteri -> errore
                printf("Parola troppo corta \n"
                        "Deve avere almeno %d caratteri \n", MIN_LUNGHEZZA_PAROLA);
                fflush(stdout);

                free(msg_stdin);
                continue;
            }
            else {
                // se la parola è valida allora viene inoltrata al server
                prepara_msg(fd_c, MSG_PAROLA, argomento);

                free(msg_stdin);
            }
        }
        // richiesta della classifica
        else if (strcmp(comando, "classifica") == 0) {
            prepara_msg(fd_c, MSG_PUNTI_FINALI, NULL);

            free(msg_stdin);
        }
        // richiesta esplicita di terminazione
        else if (strcmp(comando, "fine") == 0) {
            // comunicare al server che il client si sta chiudendo
            prepara_msg(fd_c, MSG_ERR, NULL);

            // avvisare il thread ricezione
            SYST(pthread_kill(comunicazione[1].t_id, SIGUSR2));

            // chiudere il socket
            SYSC(ret, close(fd_c), "Errore nella chiusura del socket");

            pthread_exit(NULL);
        }
        else {
            // tutte le altre richieste non sono valide
            comando_non_valido();
            continue;
        }

        memset(buffer, 0, MAX_LUNGHEZZA_STDIN);
    }
}

// thread per ricevere messaggi dal server
void *ricezione_client (void *args) {
    int ret;

    thread_args *param = (thread_args*)args;
    int fd_c = param->sck;

    // sbloccare SIGUSR2
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR2);
    SYST(pthread_sigmask(SIG_UNBLOCK, &set, NULL));

    Msg_Socket *risposta = NULL;

    // attesa della risposta dal server
    while (1) {

        risposta = ricezione_msg(fd_c);

        // chiusura del server
        if (risposta == NULL) {
            printf("Il server si sta chiudendo... \n");

            // avvisare thread invio per chiusura pulita
            SYST(pthread_kill(comunicazione[0].t_id, SIGUSR1));

            free(risposta);

            SYSC(ret, close(fd_c), "Errore nella chiusura del socket");

            pthread_exit(NULL);
        }

        // if con tutti i tipi di risposta

        // chiusura esplicita del server
        if (risposta -> type == MSG_SERVER_SHUTDOWN) {
            printf("Il server si sta chiudendo... \n");

            // avvisare thread invio per chiusura pulita
            SYST(pthread_kill(comunicazione[0].t_id, SIGUSR1));
            
            free(risposta);

            // chiudere il socket
            SYSC(ret, close(fd_c), "Errore nella chiusura del socket");

            pthread_exit(NULL);                       
        }
        // messaggio di conferma
        else if (risposta -> type == MSG_OK) {
            printf("%s\n", risposta->data);
            fflush(stdout);
        }
        // messaggio di errore
        else if (risposta -> type == MSG_ERR) {
            printf("%s\n", risposta->data);
            fflush(stdout);
        }
        // messaggio della matrice
        else if (risposta -> type == MSG_MATRICE) {
            // stampa della matrice con conversione da stringa del messaggio a 'tabella'
            printf("Matrice di gioco: \n");
            stampa_matrice_stringa(risposta->data);
        }
        // stampa del tempo rimanente
        else if (risposta -> type == MSG_TEMPO_PARTITA) {
            printf("Mancano %s secondi alla fine della partita \n", risposta->data);
        }
        // stampa del tempo della pausa
        else if (risposta -> type == MSG_TEMPO_ATTESA) {
            printf("Mancano %s secondi all'inizio della partita \n", risposta->data);
        }
        // stampa del punteggio della parola
        else if (risposta -> type == MSG_PUNTI_PAROLA) {
            printf("Hai ottenuto %s punti \n", risposta->data);
        }
        // stampa della bacheca
        else if (risposta -> type == MSG_SHOW_BACHECA) {
            if (risposta -> length == 0) {
                // se la bacheca è vuota
                printf("Bacheca vuota! \n");
                fflush(stdout);
            }
            else {
                printf("Bacheca messaggi: \n");
                // stampa della bacheca
                printf("%s\n", risposta->data);
                fflush(stdout);
            }
        }
        // stampa della classifica
        else if (risposta -> type == MSG_PUNTI_FINALI) {
            int i = 1;

            printf("Classifica finale: \n");
            fflush(stdout);
            char *token = strtok(risposta->data, "\n");

            // stampa del vincitore
            printf("Il vincitore è %s \n", token);
            fflush(stdout);

            // stampa della classifica con formato posizione - nome : punteggio
            while (token != NULL) {
                printf(" %d - %s \n", i, token);

                token = strtok(NULL, "\n");

                i++;
            }
        }
        // qualsiasi altra risposta è considerata non valida
        else {
            printf("Risposta non valida! \n");
            fflush(stdout);
            continue;
        }

        free(risposta->data);
    }
}

void client (char* nome_server, int porta_server) {
    int ret;

    struct sockaddr_in server_addr;

    // creazione socket del client
    SYSC(fd_client, socket(AF_INET, SOCK_STREAM, 0), "Errore nella socket (client)");
    // inizializzazione del socket (uguale al server)
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(porta_server);
    server_addr.sin_addr.s_addr = inet_addr(nome_server);

    // connessione sul socket su stesso indirizzo e porta del server
    SYSC(ret, connect(fd_client, (struct sockaddr *)&server_addr, sizeof(server_addr)), "Errore nella connect (client)");
}

int main(int argc, char *ARGV[]) {
    char *nome_server;
    int porta_server;

    if (argc != 3) {
        printf("Errore! Parametri: %s nome_server porta_server \n", ARGV[0]);
        fflush(stdout);
        exit(EXIT_FAILURE);
    }

    nome_server = ARGV[1];
    porta_server = atoi(ARGV[2]);

    inizializza_segnali();

    // creazione socket client
    client(nome_server, porta_server);

    // prompt e display dei comandi
    printf("%s", "Benvenuto nel gioco del Paroliere! \n");
    fflush(stdout);

    printf("%s", "Per visualizzare i comandi disponibili, digitare 'aiuto' \n");
    fflush(stdout);

    printf("%s", PAROLIERE);
    fflush(stdout);

    // allocazione spazio per la struct per i thread invio e ricezione
    SYSCN(comunicazione, (thread_args*)malloc(NUM_THREAD * sizeof(thread_args)), "Errore nella malloc");
    memset(comunicazione, 0, NUM_THREAD * sizeof(thread_args));

    // assegnazione del descrittore
    comunicazione[0].sck = fd_client;
    comunicazione[1].sck = fd_client;

    // creazione thread invio
    SYST(pthread_create(&comunicazione[0].t_id, 0, invio_client, &comunicazione[0]));

    // creazione thread ricezione
    SYST(pthread_create(&comunicazione[1].t_id, 0, ricezione_client, &comunicazione[1]));

    // attesa terminazione thread
    SYST(pthread_join(comunicazione[0].t_id, NULL));
    SYST(pthread_join(comunicazione[1].t_id, NULL));

    pthread_exit(NULL);
}