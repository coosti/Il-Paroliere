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
#include "header/trie.h"

// file contenente le funzioni per la gestione del trie, utilizzato per caricare il dizionario

// creazione di un nuovo nodo
Trie* nuovo_nodo() {
    Trie* nodo = (Trie*)malloc(sizeof(Trie));

    if (nodo == NULL) {
        perror("Errore nell'allocazione del nodo");
        exit(EXIT_FAILURE);
    }
    nodo->fine_parola = 0;

    for (int i = 0; i < MAX_CARATTERI; i++) {
        nodo->figli[i] = NULL;
    }
    return nodo;
}

// inserimento di una parola
void inserimento_trie(Trie *radice, char *parola) {

    Trie *tmp = radice;

    for (int i = 0; i < strlen(parola); i++) {
        int indice = parola[i] - 'a';
        if (tmp->figli[indice] == NULL) {
            tmp->figli[indice] = nuovo_nodo();
        }
        tmp = tmp->figli[indice];
    }

    tmp->fine_parola = 1;
}

// ricerca di una parola
int ricerca_trie(Trie *radice, char *parola) {
    Trie *tmp = radice;

    for (int i = 0; i < strlen(parola); i++) {
        int indice = parola[i] - 'a';
        if (tmp->figli[indice] == NULL) {
            return 0;
        }
        tmp = tmp->figli[indice];
    }

    if (tmp != NULL && tmp->fine_parola == 1) {
        return 1;
    }
    else
        return 0;
}

// deallocazione del trie
void deallocazione_trie(Trie *radice) {
    if (radice == NULL) {
        return;
    }

    for (int i = 0; i < MAX_CARATTERI; i++) {
        if (radice -> figli[i]) {
            deallocazione_trie(radice -> figli[i]);
        }
    }

    free(radice);
}