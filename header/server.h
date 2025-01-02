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
#include <sys/stat.h>

#include "header/macros.h"
#include "header/trie.h"
#include "header/shared.h"
#include "header/bacheca.h"
#include "header/matrice.h"

// costanti

#define N 256

#define MAX_CLIENT 32

#define MAX_MESSAGGI 8

// nodo lista thread attivi
typedef struct thread_attivo {
    pthread_t t_id;
    struct thread_attivo *next;
} thread_attivo;

// struct parametri thread
typedef struct {
    int *sck;
    // ...
    // potrebbe essere utile un puntatore alla lista in cui è contenuto?
} client_args;

// lista thread attivi
typedef struct {
    thread_attivo *head;
    int num_thread;
    // mutex per la lista
} lista_thread;


// struct giocatore
typedef struct giocatore {
    char *nome_utente;
    pthread_t t_id;
    int punteggio;
    parole_trovate *parole;
    int fd_c;
} giocatore;

// lista giocatori
typedef struct {
    giocatore *head;
    int num_giocatori;
} lista_giocatori;


// lista parole trovate da un giocatore
typedef struct parole_trovate {
    char *parola;
    int punti;
    struct parole_trovate *next;
} parole_trovate;


// coda punteggi
/*typedef struct classifica {
    giocatore *utente;
    struct classifica *next;
} classifica;*/

// funzione per il caricamento del dizionario nel trie
void caricamento_dizionario(char *file_dizionario, Trie *radice);

// funzione dei thread che gestiscono i client
void *thread_client (void *args);

// funzione del thread gioco
void *gioco (void *args);

// funzione del thread scorer
void *scorer (void *arg);

// funzioni liste

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
