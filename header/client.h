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

#define NUM_THREAD 2

#define MAX_LUNGHEZZA_STDIN 256

// struct per raggruppare i parametri utili dei thread
typedef struct {
    pthread_t t_id; // tid
    int sck;   // puntatore al file descriptor del socket
} thread_args;

void stampa_comandi();

void comando_non_valido();

int controllo_lunghezza_max (char *argomento, int max_lunghezza);

int controllo_lunghezza_min (char *argomento, int min_lunghezza);

int username_valido (char *nome_utente);

void inizializza_segnali ();

void sigint_handler (int sig);

void invio_handler (int sig);

void ricezione_handler (int sig);

void *invio_client (void *args);

void *ricezione_client (void *args);

void client (char *nome_server, int porta_server);
