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

#include <sys/socket.h> 
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "header/client.h"

// variabili globali

int fd_client;

thread_args *comunicazione;

char *PAROLIERE = "[PROMPT PAROLIERE] --> ";

// thread per leggere comandi da tastiera e inviarli al server
// SERVIREBBE UNA LOCK !!!!!
void *invio_client (void *args) {
    int ret;

    // dagli argomenti recuperare il descrittore del socket
    thread_args *param = (thread_args *)args;

    int fd_c = *(param->sck);

    while (1) {
        // lettura da standard input 
        char buffer[MAX_LUNGHEZZA_STDIN];
        ssize_t n_read;

        SYSC(n_read, read(STDIN_FILENO, buffer, MAX_LUNGHEZZA_STDIN), "Errore nella lettura da STDIN");

        // variabile per il comando richiesto, grande quanto i byte effettivamente letti 
        char *msg_stdin = (char*)malloc(n_read + 1);
        strncpy(msg_stdin, buffer, n_read);
        msg_stdin[n_read] = '\0';

        // tokenizzare il contenuto di msg_stdin per prendere il comando ed eventuali parametri previsti
        char *comando = strtok(msg_stdin, " ");
        char *argomento = strtok(NULL, " ");

        // messaggio da inviare -> indecisa se allocarla dinamicamente
        Msg_Socket richiesta;
        richiesta.type = ' ';
        richiesta.length = 0;
        // attualmente a null, alloco spazio solo quando c'è l'argomento
        richiesta.data = NULL;

        // if con tutti i casi
        if (strcmp(comando, "aiuto") == 0 && argomento == NULL) {
            stampa_comandi();
            free(msg_stdin);
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
                // se il nome utente è valido invio il messaggio
                richiesta.type = MSG_REGISTRA_UTENTE;
                richiesta.length = strlen(argomento);

                richiesta.data = (char*)malloc(richiesta.length + 1);
                strncpy(richiesta.data, argomento, richiesta.length);
                richiesta.data[richiesta.length] = '\0';

                invio_msg(fd_c, &richiesta);
                free(richiesta.data);
                free(msg_stdin);
            }
        }
        else if (strcmp(comando, "matrice") == 0 && argomento == NULL) {
            richiesta.type = MSG_MATRICE;
            invio_msg(fd_c, &richiesta);

            free(msg_stdin);
        }
        else if (strcmp(comando, "msg") == 0) {
            // prima di inviare il messaggio, controllarne la lunghezza
            if (argomento == NULL) {
                comando_non_valido();
                continue;
            }
            else if (strlen(argomento) == 0) {
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
                richiesta.type = MSG_POST_BACHECA;
                richiesta.length = strlen(argomento);
                richiesta.data = (char*)malloc(richiesta.length + 1);

                strncpy(richiesta.data, argomento, richiesta.length);
                richiesta.data[richiesta.length] = '\0';

                invio_msg(fd_c, &richiesta);

                free(richiesta.data);
                free(msg_stdin);
            }
        }
        else if (strcmp(comando, "show_msg") == 0 && argomento == NULL) {
            richiesta.type = MSG_SHOW_BACHECA;
            invio_msg(fd_c, &richiesta);

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
            else if (!(controllo_lunghezza_min(argomento, MIN_LUNGHEZZA_PAROLA))) {
                printf(" parola troppo corta \n"
                        "deve avere almeno %d caratteri \n"
                        "%s \n", MIN_LUNGHEZZA_PAROLA, PAROLIERE);
            }
            else {
                // se la parola è lunga almeno 4 caratteri, allora invio al server
                richiesta.type = MSG_PAROLA;
                richiesta.length = strlen(argomento);

                richiesta.data = (char*)malloc(richiesta.length + 1);
                strncpy(richiesta.data, argomento, richiesta.length);
                richiesta.data[richiesta.length] = '\0';

                invio_msg(fd_c, &richiesta);

                free(richiesta.data);
                free(msg_stdin);
            }
        }
        else if (strcmp(comando, "classifica") == 0) {
            richiesta.type = MSG_PUNTI_FINALI;
            invio_msg(fd_c, &richiesta);

            free(msg_stdin);
        }
        else if (strcmp(comando, "fine") == 0) {
            richiesta.type = MSG_CLIENT_SHUTDOWN;
            invio_msg(fd_c, &richiesta);

            free(msg_stdin);

            break;
            // chiusura del client -> chi se ne occupa? la deve fare il client stesso?
        }
        else {
            // è stato inserito un comando non valido
            comando_non_valido();
        }
    }

    return NULL;
}

// thread per ricevere messaggi dal server
void *ricezione_client (void *args) {
    int ret;

    // dagli argomenti recuperare il descrittore del socket
    thread_args *param = (thread_args *)args;

    int fd_c = *(param->sck);

    // attesa della risposta dal server
    while (1) {
        Msg_Socket *risposta = ricezione_msg(fd_c);

        if (risposta->data == NULL) {
            // se non si riceve nulla, continuare ad attendere
            continue;
        }
        if (risposta->type == MSG_OK) {
            printf("%s \n", risposta->data);
        }
        else if (risposta->type == MSG_ERR) {
            printf("%s\n", risposta->data);
        }
        else if (risposta->type == MSG_MATRICE) {
            // devo far diventare la risposta una matrice
            //stampa_matrice(risposta->data);
        }
        else if (risposta->type == MSG_TEMPO_PARTITA) {
            // stampa del tempo rimanente
            printf("Mancano %s minuti alla fine della partita \n", risposta->data);
        }
        else if (risposta->type == MSG_TEMPO_ATTESA) {
            // stampa del tempo della pausa
            printf("Mancano %s minuti all'inizio della partita \n", risposta->data);
        }
        else if (risposta->type == MSG_PUNTI_PAROLA) {
            // stampa punteggio parola -> funzione ????
            printf("Hai ottenuto %s punti \n", risposta->data);
        }
        else if (risposta->type == MSG_PUNTI_FINALI) {
            // funzione per la stampa della classifica
            // devo aspettare lo scorer
        }
        else if (risposta->type == MSG_SHOW_BACHECA) {
            // stampa_bacheca(risposta->data, risposta->length);
        }
        else if (risposta->type == MSG_SERVER_SHUTDOWN) {
            printf("%s \n", risposta->data); 
        }

        // se la risposta non è valida, la ignoro
        printf("%s \n", PAROLIERE);
    }

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

    char *nome_server;
    int porta_server;

    int ret;

    if (argc != 3) {
        printf("Errore! Parametri: %s nome_server porta_server \n", ARGV[0]);
        exit(EXIT_FAILURE);
    }

    nome_server = ARGV[1];
    porta_server = atoi(ARGV[2]);

    // creazione socket client
    client(nome_server, porta_server);

    // prompt e display dei comandi

    // allocazione spazio per la struct per i thread invio e ricezione
    SYSCN(comunicazione, (thread_args *)malloc(NUM_THREAD*sizeof(thread_args)), "Errore nella malloc");

    // assegnazione del descrittore
    comunicazione[0].sck = &fd_client;
    comunicazione[1].sck = &fd_client;

    // creazione thread invio
    SYST(pthread_create(&comunicazione[0].t_id, 0, invio_client, &comunicazione[0]));

    // creazione thread ricezione
    SYST(pthread_create(&comunicazione[1].t_id, 0, ricezione_client, &comunicazione[1]));

    // attesa thread
    SYST(pthread_join(comunicazione[0].t_id, NULL));
    SYST(pthread_join(comunicazione[1].t_id, NULL));

    // alla chiusura, liberare la struct

    return 0;
}