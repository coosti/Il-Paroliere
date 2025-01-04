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

// nodo lista thread attivi
typedef struct thread_attivo {
    pthread_t t_id;
    struct thread_attivo *next;
} thread_attivo;

// lista thread attivi
typedef struct {
    thread_attivo *head;
    int num_thread;
    // mutex per la lista
} lista_thread;

// lista parole trovate da un giocatore
typedef struct parola_trovata {
    char *parola;
    int punti;
    struct parola_trovata *next;
} parola_trovata;

typedef struct lista_parole {
    parola_trovata *head;
    int num_parole;
} lista_parole;

// struct giocatore
typedef struct giocatore {
    char *username;
    pthread_t t_id;
    int punteggio;
    lista_parole *parole_trovate;
    int fd_c;
    struct giocatore *next;
} giocatore;

// lista giocatori
typedef struct {
    giocatore *head;
    int num_giocatori;
} lista_giocatori;


// funzioni liste

// funzioni lista di thread
void inizializza_lista_thread (lista_thread *lista);

void inserisci_thread (lista_thread *lista, pthread_t tid);

void rimuovi_thread (lista_thread *lista, pthread_t tid);

void svuota_lista_thread (lista_thread *lista);


// funzioni lista di giocatori
void inizializza_lista_giocatori (lista_giocatori *lista);

void inserisci_giocatore (lista_giocatori *lista, char *nome_utente, int fd);

void rimuovi_giocatore (lista_giocatori *lista, char *nome_utente);

int cerca_giocatore (lista_giocatori *lista, char *nome_utente);

void svuota_lista_giocatori (lista_giocatori *lista);


// funzioni lista parole
void inizializza_parole (lista_parole *lista);

void inserisci_parola (lista_parole *lista, char *parola, int punti);

int cerca_parola (lista_parole *lista, char *parola);

void svuota_lista_parole (lista_parole *lista);