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

void prepara_msg(int fd, char type, char *m) {
    // struct per inviare il messaggio
    Msg_Socket invio;

    // preparazione del messaggio
    invio.type = type;
    if (m == NULL) {
        invio.length = 0;
        invio.data = NULL;
    }
    else {
        invio.length = strlen(m);

        SYSCN(invio.data, (char*)malloc(invio.length + 1), "Errore nell'allocazione del campo data");
        strncpy(invio.data, m, invio.length);
        invio.data[invio.length] = '\0';
    }

    // viene passato il msg_socket alla funzione di invio
    invio_msg(fd, &invio);

    free(invio.data);
    invio.type = ' ';
    invio.length = 0;
}

// funzione di invio -> fin qui nessun problema
void invio_msg(int fd, Msg_Socket *msg) {
    int ret;
    // riprendere i campi da msg

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
    if (n_read == 0) {
        free(msg);
        return NULL;
    }
    msg->type = t;

    // lettura della lunghezza del messaggio
    SYSC(n_read, read(fd, &l, sizeof(l)), "Errore nella read della lunghezza del messaggio");
    if (n_read == 0) {
        free(msg);
        return NULL;
    }
    msg->length = l;

    if (l > 0) {
        // se la lunghezza del messaggio è maggiore di 0 si alloca il campo data grande esattamente quanto specificato
        // (+1 per il terminatore della stringa)
        SYSCN(msg->data, (char*)malloc(l + 1), "Errore nell'allocazione del campo data");

        // lettura del messaggio
        SYSC(n_read, read(fd, msg->data, l), "Errore nella read del messaggio");
        if (n_read == 0) {
            free(msg->data);
            free(msg);
            return NULL;
        }

        // terminatore della stringa
        msg->data[l] = '\0';
    }
    else if (l == 0) {
        // con lunghezza uguale a 0, si alloca il campo data con NULL
        msg->data = NULL;
    }

    return msg;
}