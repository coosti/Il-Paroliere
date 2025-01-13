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

int fd_client;

thread_args *comunicazione;

char *PAROLIERE = "[PROMPT PAROLIERE] --> ";

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
    sa_int.sa_flags = 0;
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

// handler per sigint -> chiusura del client
void sigint_handler (int sig) {
    int ret; 
    printf("sigint ricevuto devo chiudere \n");

    // comunicare al server che il client si sta chiudendo
    prepara_msg(fd_client, MSG_CLIENT_SHUTDOWN, NULL);
    
    // eliminare thread invio
    SYST(pthread_cancel(comunicazione[0].t_id));

    // eliminare thread ricezione
    SYST(pthread_cancel(comunicazione[1].t_id));
    
    shutdown(fd_client, SHUT_RDWR);

    // chiusura del socket
    SYSC(ret, close(fd_client), "Errore nella chiusura del socket");
    
    // liberare struct comunicazione dei thread
    if (comunicazione != NULL) {
        free(comunicazione);
    }
    
    exit(0);
}

// segnali per chiudersi reciprocamente

// handler per sigusr1
void invio_handler (int sig) {
    printf("ricevuto sigusr1\n");

    // inviare messaggio di chiusura al server
    prepara_msg(fd_client, MSG_CLIENT_SHUTDOWN, NULL);

    // terminazione
    pthread_exit(NULL);
    return;
}

// handler per sigusr2
void ricezione_handler (int sig) {
    printf("ricevuto sigusr2 !!!\n");

    // non deve fare nulla perché non deve più ricevere
    
    // terminazione
    pthread_exit(NULL);
    return;
}

// thread per leggere comandi da tastiera e inviarli al server
void *invio_client (void *args) {

    // dagli argomenti recuperare il descrittore del socket
    thread_args *param = (thread_args *)args;

    int fd_c = param->sck;

    // sbloccare SIGUSR1
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    SYST(pthread_sigmask(SIG_UNBLOCK, &set, NULL));

    printf("ciao sono thread invio tid: %ld \n", pthread_self());

    while (1) {
        // lettura da standard input 
        char buffer[MAX_LUNGHEZZA_STDIN];
        ssize_t n_read;

        SYSC(n_read, read(STDIN_FILENO, buffer, MAX_LUNGHEZZA_STDIN - 1), "Errore nella lettura da STDIN");
        buffer[n_read] = '\0';

        buffer[strcspn(buffer, "\n")] = '\0';

        // variabile per il comando richiesto, grande quanto i byte effettivamente letti 
        char *msg_stdin;
        SYSCN(msg_stdin, (char*)malloc(n_read + 1), "Errore nella malloc");

        strncpy(msg_stdin, buffer, n_read);
        msg_stdin[n_read] = '\0';

        // tokenizzare il contenuto di msg_stdin per prendere il comando ed eventuali parametri previsti
        char *comando = strtok(msg_stdin, " ");
        char *argomento = strtok(NULL, " ");

        // if con tutti i casi
        if (strcmp(comando, "aiuto") == 0 && argomento == NULL) {
            stampa_comandi();
            free(msg_stdin);
            continue;
        }
        else if (strcmp(comando, "registra_utente") == 0) {
            // prima di inviare il messaggio, controllare la validità del nome utente
            // lunghezza compresa tra 3 e 10, controllo sui caratteri alfanumerici e tutto minuscolo
            if (argomento == NULL) {
                // se il parametro non viene inserito, stampare messaggio di errore e dare la possibilità di inviare un altro comando
                comando_non_valido();
                free(msg_stdin);
                continue;
            }
            else if (!(controllo_lunghezza_min(argomento, MIN_LUNGHEZZA_USERNAME))) {
                printf("nome utente troppo corto! \n"
                        "deve avere almeno %d caratteri \n"
                        "%s \n", MIN_LUNGHEZZA_USERNAME, PAROLIERE);
            }
            else if (!(controllo_lunghezza_max(argomento, MAX_LUNGHEZZA_USERNAME))) {
                printf("nome utente troppo lungo! \n"
                        "deve avere massimo %d caratteri \n"
                        "%s \n", MAX_LUNGHEZZA_USERNAME, PAROLIERE);
            }
            else if (!(username_valido(argomento))) {
                printf("nome utente non valido! \n"
                        "deve essere tutto minuscolo e contenere solo caratteri alfanumerici \n"
                        "%s \n", PAROLIERE);
            }
            else {

                printf("nome utente valido! %s \n", argomento);

                // se il nome utente è valido invio il messaggio con l'username
                prepara_msg(fd_c, MSG_REGISTRA_UTENTE, argomento);

                free(msg_stdin);
            }
        }
        else if (strcmp(comando, "matrice") == 0 && argomento == NULL) {
            prepara_msg(fd_c, MSG_MATRICE, NULL);

            free(msg_stdin);
        }
        else if (strcmp(comando, "msg") == 0) {
            // prima di inviare il messaggio, controllarne la lunghezza
            if (argomento == NULL) {
                comando_non_valido();
                continue;
            }

            // iterare con strtok per prendere tutto il messaggio
            char *msg_intero = strtok(NULL, "");
            if (msg_intero != NULL) {
                size_t len = strlen(argomento) + strlen(msg_intero) + 2; // Spazio per concatenazione e terminatore
                char *full_argomento;
                SYSCN(full_argomento, (char *)malloc(len), "Errore nell'allocazione della memoria");
                
                snprintf(full_argomento, len, "%s %s", argomento, msg_intero);
                argomento = full_argomento; // Usa la stringa concatenata come nuovo argomento
            }

            if (strlen(argomento) == 0) {
                printf("messaggio vuoto! \n"
                        "%s \n", PAROLIERE);
            }
            else if (!(controllo_lunghezza_max(argomento, MAX_CARATTERI_MESSAGGIO))) {
                printf("messaggio troppo lungo! \n"
                        "massimo %d caratteri \n"
                        "%s \n", MAX_CARATTERI_MESSAGGIO, PAROLIERE);
            }
            else {
                // se il messaggio è valido lo invio
                prepara_msg(fd_c, MSG_POST_BACHECA, argomento);

                free(msg_stdin);
            }
        }
        else if (strcmp(comando, "show_msg") == 0 && argomento == NULL) {
            prepara_msg(fd_c, MSG_SHOW_BACHECA, NULL);

            free(msg_stdin);
        }
        else if (strcmp(comando, "p") == 0) {
            // controllare la lunghezza minima della parola
            // controlli su esistenza nella matrice e nel dizionario li fa il SERVER
            if (argomento == NULL) {
                comando_non_valido();
                free(msg_stdin);
                continue;
            }
            else if (strtok(NULL, " ") != NULL) {
                // se c'è più di una parola dopo p -> errore
                printf("Errore: il comando 'p' accetta un solo argomento! \n");
                free(msg_stdin);
                continue;
            }
            else if (!(controllo_lunghezza_min(argomento, MIN_LUNGHEZZA_PAROLA))) {
                // se la parola ha meno di 4 caratteri -> errore
                printf(" parola troppo corta \n"
                        "deve avere almeno %d caratteri \n"
                        "%s \n", MIN_LUNGHEZZA_PAROLA, PAROLIERE);
            }
            else {
                // se la parola è lunga almeno 4 caratteri, allora invio al server
                prepara_msg(fd_c, MSG_PAROLA, argomento);

                free(msg_stdin);
            }
        }
        else if (strcmp(comando, "classifica") == 0) {
            prepara_msg(fd_c, MSG_PUNTI_FINALI, NULL);

            free(msg_stdin);
        }
        else if (strcmp(comando, "fine") == 0) {
            // comunicare al server che il client si sta chiudendo
            prepara_msg(fd_c, MSG_CLIENT_SHUTDOWN, NULL);

            // avviso il thread di ricezione della chiusura del client
            SYST(pthread_kill(comunicazione[1].t_id, SIGUSR2));

            pthread_exit(NULL);

            free(msg_stdin);

            break;
        }
        else {
            // è stato inserito un comando non valido
            comando_non_valido();
        }

        memset(buffer, 0, MAX_LUNGHEZZA_STDIN);
    }

    printf("ciao mi sono chiuso invio \n");

    return NULL;
}

// thread per ricevere messaggi dal server
void *ricezione_client (void *args) {

    // dagli argomenti recuperare il descrittore del socket
    thread_args *param = (thread_args*)args;

    int fd_c = param->sck;

    // sbloccare SIGUSR2
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR2);
    SYST(pthread_sigmask(SIG_UNBLOCK, &set, NULL));

    Msg_Socket *risposta = NULL;

    printf("ciao sono io ricevitore tid: %ld \n", pthread_self());

    // attesa della risposta dal server
    while (1) {
        // se la risposta non è valida, la ignoro
        printf("%s \n", PAROLIERE);

        risposta = ricezione_msg(fd_c);

        if (risposta -> data == NULL) {
            // se non si riceve nulla, continuare ad attendere
            continue;
        }
        if (risposta -> type == MSG_OK) {
            printf("%s \n", risposta->data);
        }
        else if (risposta -> type == MSG_ERR) {
            printf("%s\n", risposta->data);
        }
        else if (risposta -> type == MSG_MATRICE) {
            // stampa della matrice
            printf("Matrice di gioco: \n");
            stampa_matrice_stringa(risposta->data);
        }
        else if (risposta -> type == MSG_TEMPO_PARTITA) {
            // stampa del tempo rimanente
            printf("Mancano %s secondi alla fine della partita \n", risposta->data);
        }
        else if (risposta -> type == MSG_TEMPO_ATTESA) {
            // stampa del tempo della pausa
            printf("Mancano %s secondi all'inizio della partita \n", risposta->data);
        }
        else if (risposta -> type == MSG_PUNTI_PAROLA) {
            // stampa punteggio parola -> funzione ????
            printf("Hai ottenuto %s punti \n", risposta->data);
        }
        else if (risposta -> type == MSG_PUNTI_FINALI) {
            // stampa della classifica
            int i = 1;

            printf("Classifica finale: \n");
            char *token = strtok(risposta->data, "\n");

            printf("Il vincitore è: %s \n", token);

            while (token != NULL) {
                printf(" %d %s \n", i, token);

                token = strtok(NULL, "\n");

                i++;
            }
        }
        else if (risposta -> type == MSG_SHOW_BACHECA) {
            if (risposta -> length == 0) {
                printf("Bacheca vuota! \n");
            }
            else {
                printf("Bacheca messaggi: \n");
                // stampa della bacheca
                printf("%s \n", risposta->data);
            }
        }
        else if (risposta -> type == MSG_SERVER_SHUTDOWN) {
            // il server si sta chiudendo, devo chiudere anche io client
            printf("Il server si sta chiudendo... \n");

            // avviso al thread di invio della chiusura del server
            SYST(pthread_kill(comunicazione[0].t_id, SIGUSR1));

            pthread_exit(NULL);
        }
        else {
            printf("Risposta non valida! \n");
        }

        free(risposta->data);
    }

    free(risposta);

    printf("ciao mi sono chiuso ricezione \n");

    return NULL;
}

void client (char* nome_server, int porta_server) {
    int ret;

    struct sockaddr_in server_addr;

    // creazione socket client
    SYSC(fd_client, socket(AF_INET, SOCK_STREAM, 0), "Errore nella socket (client)");

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(porta_server);
    server_addr.sin_addr.s_addr = inet_addr(nome_server);

    // connessione
    SYSC(ret, connect(fd_client, (struct sockaddr *)&server_addr, sizeof(server_addr)), "Errore nella connect (client)");
}

int main(int argc, char *ARGV[]) {
    int ret;

    char *nome_server;
    int porta_server;

    if (argc != 3) {
        printf("Errore! Parametri: %s nome_server porta_server \n", ARGV[0]);
        exit(EXIT_FAILURE);
    }

    nome_server = ARGV[1];
    porta_server = atoi(ARGV[2]);

    // creazione socket client
    client(nome_server, porta_server);

    // prompt e display dei comandi
    char *msg = "Benvenuto! \n";
    SYSC(ret, write(STDOUT_FILENO, msg, strlen(msg)), "Errore nella write");

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

    // attesa thread
    SYST(pthread_join(comunicazione[0].t_id, NULL));
    SYST(pthread_join(comunicazione[1].t_id, NULL));

    free(comunicazione);

    return 0;
}