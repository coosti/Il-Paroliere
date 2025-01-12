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
#include "header/macros.h"

// implementazione delle funzioni per richieste e risposte nella forma corretta

void stampa_comandi() {
    printf("\ncomandi disponibili: \n"
            "\t aiuto - richiedere i comandi disponibili \n"
            "\t registra_utente <nome_utente> - registrazione al gioco \n"
            "\t matrice - richiedere la matrice corrente e il tempo\n"
            "\t msg <testo> - postare un messaggio sulla bacheca \n"
            "\t show_msg - stampa della bacheca \n"
            "\t p <parola> - proporre una parola \n"
            "\t classifica - richiedere la classifica \n"
            "\t fine - uscire dal gioco \n"
            "\n");
}

void comando_non_valido() {
    printf("richiesta non valida! \n"
            "per visualizzare i comandi, digitare 'aiuto' \n"
            "[PROMPT PAROLIERE] -->  \n");
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
        if (!isalnum(*nome_utente) && !islower(*nome_utente)) {
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
        risposta.data = NULL;
    }
    else {
        risposta.length = strlen(m);

        SYSCN(risposta.data, (char*)malloc(risposta.length + 1), "Errore nell'allocazione del campo data");
        strncpy(risposta.data, m, risposta.length);
        risposta.data[risposta.length] = '\0';
    }

    // viene passato il msg_socket alla funzione di invio
    invio_msg(fd, &risposta);

    free(risposta.data);
    risposta.type = ' ';
    risposta.length = 0;
}

// funzione di invio -> fin qui nessun problema
void invio_msg(int fd, Msg_Socket *msg) {
    int ret;
    // riprendere i campi da msg

    /*printf("tipo messaggio (shared.c): %c \n", msg->type);

    printf("lunghezza messaggio (shared.c): %d \n", msg->length);

    printf("messaggio (shared.c): %s \n", msg->data);*/

    // invio del carattere che indica il tipo del messaggio
    SYSC(ret, write(fd, &msg->type, sizeof(char)), "Errore nella write del tipo di messaggio");

    // invio della lunghezza del messaggio
    SYSC(ret, write(fd, &msg->length, sizeof(int)), "Errore nella write della lunghezza del messaggio");

    // invio del messaggio
    if (msg->length > 0 && msg->data != NULL) {
        SYSC(ret, write(fd, msg->data, msg->length), "Errore nella write del messaggio");
    }

    return;
}

// funzione di ricezione
Msg_Socket *ricezione_msg(int fd) {
    ssize_t n_read;
    char t;
    int l;

    // allocazione della struct per il messaggio (per restituirne il puntatore)
    Msg_Socket *msg;    
    SYSCN(msg, (Msg_Socket*)malloc(sizeof(Msg_Socket)), "Errore nella malloc del messaggio");

    memset(msg, 0, sizeof(Msg_Socket));

    // lettura del carattere che indica il tipo del messaggio
    SYSC(n_read, read(fd, &t, sizeof(t)), "Errore nella read del tipo di messaggio");
    msg->type = t;

    // lettura della lunghezza del messaggio
    SYSC(n_read, read(fd, &l, sizeof(l)), "Errore nella read della lunghezza del messaggio");
    msg->length = l;

    if (l > 0) {
        // se la lunghezza del messaggio è maggiore di 0 si alloca il campo data grande esattamente quanto specificato
        // (+1 per il terminatore della stringa)
        SYSCN(msg->data, (char*)malloc(l + 1), "Errore nell'allocazione del campo data");

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