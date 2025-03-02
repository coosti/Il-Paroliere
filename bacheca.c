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

#include "header/bacheca.h"
#include "header/shared.h"

// file contenente le funzioni per la gestione della bacheca

Bacheca* allocazione_bacheca() {
    Bacheca *bacheca = malloc(sizeof(Bacheca));
    if (bacheca == NULL) {
        perror("Errore allocazione bacheca");
        exit(EXIT_FAILURE);
    }

    // allocazione array di struct
    bacheca -> messaggi  = malloc(MAX_MESSAGGI * sizeof(Messaggio));
    if (bacheca -> messaggi == NULL) {
        perror("Errore allocazione bacheca");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < MAX_MESSAGGI; i++) {
        bacheca -> messaggi[i].nome_utente = malloc(MAX_LUNGHEZZA_USERNAME + 1);
        if (bacheca -> messaggi[i].nome_utente == NULL) {
            perror("Errore allocazione nome_utente");
            exit(EXIT_FAILURE);
        }

        bacheca -> messaggi[i].messaggio = malloc(MAX_CARATTERI_MESSAGGIO + 1);
        if (bacheca -> messaggi[i].messaggio == NULL) {
            perror("Errore allocazione messaggio");
            exit(EXIT_FAILURE);
        }
    }

    bacheca -> num_msg = 0;

    return bacheca;
}

// post del messaggio nella bacheca dopo MSG_POST_BACHECA
void inserimento_bacheca (Bacheca *bacheca, char *username, char *msg) {
    // variabile usata per capire fino a dove far scorrere il ciclo for per lo shift
    // si distinguono i casi bacheca piena o vuota
    int k = 0;
    // bacheca piena
    if (bacheca -> num_msg == MAX_MESSAGGI) {
        k = MAX_MESSAGGI - 1;
    }
    else {
        k = bacheca -> num_msg;
    }

    // shift dei messaggi già presenti in avanti
    // nel caso di bacheca piena, shiftando in avanti il messaggio in ultima posizione viene automaticamente perso
    for (int i = k; i > 0; i--) {
        strncpy(bacheca -> messaggi[i].nome_utente, bacheca -> messaggi[i-1].nome_utente, MAX_LUNGHEZZA_USERNAME);
        bacheca -> messaggi[i].nome_utente[MAX_LUNGHEZZA_USERNAME] = '\0';

        strncpy(bacheca -> messaggi[i].messaggio, bacheca -> messaggi[i-1].messaggio, MAX_CARATTERI_MESSAGGIO);
        bacheca -> messaggi[i].messaggio[MAX_CARATTERI_MESSAGGIO] = '\0';
    }

    // inserimento del nuovo messaggio in posizione 0
    strncpy(bacheca -> messaggi[0].nome_utente, username, MAX_LUNGHEZZA_USERNAME);
    bacheca -> messaggi[0].nome_utente[MAX_LUNGHEZZA_USERNAME] = '\0';

    strncpy(bacheca -> messaggi[0].messaggio, msg, MAX_CARATTERI_MESSAGGIO);
    bacheca -> messaggi[0].messaggio[MAX_CARATTERI_MESSAGGIO] = '\0';

    // bacheca non ancora piena
    if (bacheca -> num_msg < MAX_MESSAGGI) {
        // si incrementa semplicemente il numero dei messaggi
        bacheca -> num_msg++;
    }
}

void stampa_bacheca(Bacheca *bacheca) {

    for (int i = 0; i < bacheca -> num_msg; i++) {
        printf("%s, %s\n", bacheca -> messaggi[i].nome_utente, bacheca -> messaggi[i].messaggio);
    }
    
}

// stampa della bacheca in csv dopo MSG_SHOW_BACHECA
char *bacheca_a_stringa(Bacheca *bacheca) {
    char *stringa = malloc(MAX_MESSAGGI * (MAX_LUNGHEZZA_USERNAME + MAX_CARATTERI_MESSAGGIO));
    if (stringa == NULL) {
        perror("Errore allocazione stringa per la bacheca");
        exit(EXIT_FAILURE);
    }

    stringa[0] = '\0';

    for (int i = 0; i < bacheca -> num_msg; i++) {
        strcat(stringa, bacheca -> messaggi[i].nome_utente);
        strcat(stringa, ", ");
        strcat(stringa, bacheca -> messaggi[i].messaggio);
        strcat(stringa, "\n");
    }

    return stringa;
}

// liberare memoria bacheca
void deallocazione_bacheca(Bacheca *bacheca) {
    for (int i =0; i<MAX_MESSAGGI; i++) {
        free(bacheca -> messaggi[i].nome_utente);
        free(bacheca -> messaggi[i].messaggio);
    }

    free(bacheca -> messaggi);
    free(bacheca);
}