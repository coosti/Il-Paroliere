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

Messaggio* allocazione_bacheca() {
    // allocazione array di struct
    Messaggio* bacheca = malloc(MAX_MESSAGGI * sizeof(Messaggio));
    if (bacheca == NULL) {
        perror("Errore allocazione bacheca");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < MAX_MESSAGGI; i++) {
        bacheca[i].nome_utente = malloc(MAX_LUNGHEZZA_USERNAME);
        if (bacheca[i].nome_utente == NULL) {
            perror("Errore allocazione nome_utente");
            exit(EXIT_FAILURE);
        }

        bacheca[i].messaggio = malloc(MAX_CARATTERI_MESSAGGIO);
        if (bacheca[i].messaggio == NULL) {
            perror("Errore allocazione messaggio");
            exit(EXIT_FAILURE);
        }
    }

    return bacheca;
}

// post del messaggio nella bacheca dopo MSG_POST_BACHECA
void inserimento_bacheca(Messaggio bacheca[], char *username, char *msg, int *num_msg) {
    if (*num_msg < MAX_MESSAGGI) {
        // bacheca ancora libera
        
        // shift dei messaggi in avanti
        for (int i = *num_msg; i > 0; i--) {
            strncpy(bacheca[i].nome_utente, bacheca[i-1].nome_utente, MAX_LUNGHEZZA_USERNAME);
            bacheca[i].nome_utente[MAX_LUNGHEZZA_USERNAME] = '\0';

            strncpy(bacheca[i].messaggio, bacheca[i-1].messaggio, MAX_CARATTERI_MESSAGGIO);
            bacheca[i].messaggio[MAX_CARATTERI_MESSAGGIO] = '\0';
        }

        // inserimento in posizione 0
        strncpy(bacheca[0].nome_utente, username, MAX_LUNGHEZZA_USERNAME);
        bacheca[0].nome_utente[MAX_LUNGHEZZA_USERNAME] = '\0';

        strncpy(bacheca[0].messaggio, msg, MAX_CARATTERI_MESSAGGIO);
        bacheca[0].messaggio[MAX_CARATTERI_MESSAGGIO] = '\0';

        (*num_msg)++;
    }
    else {
        // bacheca piena

        // shift in avanti -> messaggio in ultima posizione va perso
        for (int i = MAX_MESSAGGI-1; i > 0; i--) {
            strncpy(bacheca[i].nome_utente, bacheca[i-1].nome_utente, MAX_LUNGHEZZA_USERNAME);
            bacheca[i].nome_utente[MAX_LUNGHEZZA_USERNAME] = '\0';

            strncpy(bacheca[i].messaggio, bacheca[i-1].messaggio, MAX_CARATTERI_MESSAGGIO);
            bacheca[i].messaggio[MAX_CARATTERI_MESSAGGIO] = '\0';
        }

        // inserimento in posizione 0
        strncpy(bacheca[0].nome_utente, username, MAX_LUNGHEZZA_USERNAME);
        bacheca[0].nome_utente[MAX_LUNGHEZZA_USERNAME] = '\0';

        strncpy(bacheca[0].messaggio, msg, MAX_CARATTERI_MESSAGGIO);
        bacheca[0].messaggio[MAX_CARATTERI_MESSAGGIO] = '\0';
    }
}

// stampa della bacheca in csv dopo MSG_SHOW_BACHECA
void stampa_bacheca(Messaggio bacheca[], int *num_msg) {

    for (int i = 0; i < *num_msg; i++) {
        printf("%s, %s\n", bacheca[i].nome_utente, bacheca[i].messaggio);
    }
    
}

char *bacheca_a_stringa(Messaggio bacheca[], int *num_msg) {
    char *stringa = malloc(MAX_MESSAGGI * (MAX_LUNGHEZZA_USERNAME + MAX_CARATTERI_MESSAGGIO));
    if (stringa == NULL) {
        perror("Errore allocazione stringa per la bacheca");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < *num_msg; i++) {
        strcat(stringa, bacheca[i].nome_utente);
        strcat(stringa, ", ");
        strcat(stringa, bacheca[i].messaggio);
        strcat(stringa, "\n");
    }

    return stringa;
}

// liberare memoria bacheca
void deallocazione_bacheca(Messaggio bacheca[], int *num_msg) {
    for (int i =0; i<*num_msg; i++) {
        free(bacheca[i].nome_utente);
        free(bacheca[i].messaggio);
    }

    free(bacheca);
}



