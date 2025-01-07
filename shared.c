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

#include "header/shared.h"

// implementazione delle funzioni per richieste e risposte nella forma corretta

void stampa_comandi() {
    printf("\n ------ comandi disponibili ------ \n"
            "aiuto - richiedere i comandi disponibili \n"
            "registra_utente <nome_utente> - registrazione al gioco \n"
            "matrice - richiedere la matrice corrente e il tempo\n"
            "msg <testo> - postare un messaggio sulla bacheca \n"
            "show_msg - stampa della bacheca \n"
            "p <parola> - proporre una parola \n"
            "classifica - richiedere la classifica \n"
            "fine - uscire dal gioco \n"
            "---------------------------------- \n");
}

void comando_non_valido() {
    printf("richiesta non valida! \n"
            "per visualizzare i comandi, digitare 'aiuto' \n"
            "%s \n", PAROLIERE);
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
        if (!isalnum(*nome_utente) || !islower(*nome_utente)) {
            return 0;
        }
        nome_utente++;
    }
    return 1;
}

void prepara_msg(int fd, char type, char *m) {
    // variabile per inviare il messaggio
    Msg_Socket risposta;

    // preparazione del messaggio
    risposta.type = type;
    if (m == NULL) {
        risposta.length = 0;
    }
    else {
        risposta.length = strlen(m);

        risposta.data = (char*)malloc(risposta.length + 1);
        strncpy(risposta.data, m, risposta.length);
        risposta.data[risposta.length] = '\0';
    }

    // viene passato il msg_socket alla funzione di invio
    invio_msg(fd, &risposta);

    free(risposta.data);
    risposta.type = ' ';
    risposta.length = 0;
}

// funzione di invio
void invio_msg(int fd, Msg_Socket *msg) {
    int ret;
    // riprendere i campi da msg

    // invio del carattere che indica il tipo del messaggio
    SYSC(ret, write(fd, &msg->type, sizeof(char)), "Errore nella write del tipo di messaggio");

    // invio della lunghezza del messaggio
    SYSC(ret, write(fd, &msg->length, sizeof(int)), "Errore nella write della lunghezza del messaggio");

    // invio del messaggio
    SYSC(ret, write(fd, &msg->data, msg->length), "Errore nella write del messaggio");

    return;
}

// funzione di ricezione
Msg_Socket* ricezione_msg(int fd) {
    ssize_t n_read;
    char t;
    int l;

    // allocazione della struct per il messaggio (per restituirne il puntatore)
    Msg_Socket *msg = (Msg_Socket*)malloc(sizeof(Msg_Socket));
    if (msg == NULL) {
        perror("Errore nell'allocazione della struct per la risposta");
        exit(EXIT_FAILURE);
    }

    // lettura del carattere che indica il tipo del messaggio
    SYSC(n_read, read(fd, &t, sizeof(char)), "Errore nella read del tipo di messaggio");
    msg->type = t;

    // lettura della lunghezza del messaggio
    SYSC(n_read, read(fd, &l, sizeof(int)), "Errore nella read della lunghezza del messaggio");
    msg->length = l;

    if (l > 0) {(
        // se la lunghezza del messaggio è maggiore di 0 si alloca il campo data grande esattamente quanto specificato
        // (si aggiunge +1 per il terminatore della stringa)
        msg->data = (char*)malloc(l + 1));
        if (msg->data == NULL) {
            perror("Errore nell'allocazione del campo data");
            free(msg);
            exit(EXIT_FAILURE);
        }

        // lettura del messaggio
        SYSC(n_read, read(fd, msg->data, l), "Errore nella read del messaggio");

        // terminatore della stringa
        msg->data[l] = '\0';
    }
    else if (l == 0) {
        // con lunghezza uguale a 0, si alloca il campo data con NULL
        msg->data = NULL;
    }

    return msg;
}