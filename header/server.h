#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h> 
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <getopt.h>
#include <signal.h>
#include <sys/socket.h> 
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#include "macros.h"
#include "trie.h"
#include "shared.h"
#include "liste.h"
#include "bacheca.h"
#include "matrice.h"

// costanti

#define N 256

#define MAX_CLIENT 32

#define MAX_MESSAGGI 8

#define MAX_DIM 1024

// struct parametri thread client
typedef struct client_args {
    pthread_t t_id;
    int sck;
    // ...
    // potrebbe essere utile un puntatore alla lista in cui è contenuto?
} client_args;

// funzione per il caricamento del dizionario nel trie
void caricamento_dizionario(char *file_dizionario, Trie *radice);

char *tempo_rimanente (int minuti);

// funzione dei thread che gestiscono i client
void *thread_client (void *args);

// funzione del thread gioco
void *gioco (void *args);

// funzione del thread scorer
void *scorer (void *arg);

// funzione controllo nome utente
int controllo_username (char *nome_utente);

// funzione controllo validità parola
int controllo_parola (char *parola);

// funzione calcolo tempo

// funzione sorting per la classifica
int sorting_classifica(const void *a, const void *b);

// funzione stampa della classifica
void stampa_classifica(giocatore *head);

void server (char *nome_server, int porta_server);
