#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h> 
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <getopt.h> 

#include <sys/socket.h> 
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "header/macros.h"
#include "header/client.h"
#include "header/shared.h"
#include "header/matrice.h"

// variabili globali

int fd_client;

thread_arg *comunicazione;

// thread per leggere comandi da tastiera e inviarli al server
void *invio_client (void *args) {
    int ret;

    // dagli argomenti recuperare il descrittore del socket
    thread_arg *param = (thread_arg *)args;

    int fd_c = *(param->sck);

    while (1) {
        // lettura da standard input
        char msg_stdin[MAX_LUNGHEZZA_STDIN];
        ssize_t n_read;

        SYSC(n_read, read(STDIN_FILENO, msg_stdin, MAX_LUNGHEZZA_STDIN), "Errore nella lettura da STDIN");

        msg_stdin[n_read] = '\0';

        // in msg_stdin c'è il comando richiesto dall'utente

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
        if (strcmp(comando, "aiuto") && argomento == NULL) {
            // stampa dei comandi
        }
        else if (strcmp(comando, "registra_utente")) {
            // controlli su argomento
            // validità del nome utente
        }
        else if (strcmp(comando, "matrice") && argomento == NULL) {
            richiesta.type = MSG_MATRICE;
            invio_msg(fd_c, &richiesta);
        }
        else if (strcmp(comando, "msg")) {
            // controlli sul testo del messaggio
        }
        else if (strcmp(comando, "show_msg") && argomento == NULL) {
            richiesta.type = MSG_SHOW_BACHECA;
            invio_msg(fd_c, &richiesta);
        }
        else if (strcmp(comando, "p")) {
            // controlli sulla parola
        }
        else if (strcmp(comando, "classifica")) {
            richiesta.type = MSG_PUNTI_FINALI;
            invio_msg(fd_c, &richiesta);
        }
        else if (strcmp(comando, "fine")) {
            // comando di chiusura del client
        }
        else {
            // è stato inserito un comando non valido
            printf("Richiesta non valida! \n");
            printf("Per visualizzare i comandi disponibili, digitare 'aiuto' \n");
            printf("[PROMPT PAROLIERE] --> ");
        }



    }

    // chiusura del thread
}

// thread per ricevere messaggi dal server
void *ricezione_client (void *args) {
    int ret;

    // dagli argomenti recuperare il descrittore del socket
    thread_arg *param = (thread_arg *)args;

    int fd_c = *(param->sck);

    while (1) {
        Msg_Socket *risposta = ricezione_msg(fd_c);

        if (risposta->data == NULL) {
            // non ho ricevuto nulla dal server
            // che faccio?
        }

        if (risposta->type == MSG_OK) {
            printf("%s \n", risposta->data);
        }
        else if (risposta->type == MSG_ERR) {
            printf("Errore: %s\n", risposta->data);
        }
        else if (risposta->type == MSG_MATRICE) {
            stampa_matrice(risposta->data);
        }
        else if (risposta->type == MSG_TEMPO_PARTITA) {
            // stampa del tempo rimanente
        }
        else if (risposta->type == MSG_TEMPO_ATTESA) {
            // stampa del tempo della pausa
        }
        else if (risposta->type == MSG_PUNTI_PAROLA) {
            // stampa punteggio parola
        }
        else if (risposta->type == MSG_PUNTI_FINALI) {
            // stampa classifica
        }
        else if (risposta->type == MSG_SHOW_BACHECA) {
            // stampa_bacheca(risposta->data, risposta->length);
        }
        else if (risposta->type == MSG_SERVER_SHUTDOWN) {
            // il server è stato chiuso, chiudere il client 
        }
        else {
            // risposta non valida
        }


    }

    // chiusura del thread

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

    // allocazione spazio per la struct per i thread invio e ricezione
    SYSCN(comunicazione, (thread_arg *)malloc(NUM_THREAD*sizeof(thread_arg)), "Errore nella malloc");

    // assegnazione del descrittore
    comunicazione[0].sck = &fd_client;
    comunicazione[1].sck = &fd_client;

    // creazione thread invio
    SYST(pthread_create(&comunicazione[0].t_id, 0, invio_msg, &comunicazione[0]));

    // creazione thread ricezione
    SYST(pthread_create(&comunicazione[1].t_id, 0, ricezione_msg, &comunicazione[1]));

    // attesa thread
    SYST(pthread_join(comunicazione[0].t_id, NULL));
    SYST(pthread_join(comunicazione[1].t_id, NULL));

    // alla chiusura, liberare la struct

    return 0;
}