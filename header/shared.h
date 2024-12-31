// file per costanti, strutture dati e funzioni in comune tra client e server

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

// messaggi
#define MSG_OK 'K'
#define MSG_ERR 'E'
#define MSG_REGISTRA_UTENTE 'R'
#define MSG_MATRICE 'M'
#define MSG_TEMPO_PARTITA 'T'
#define MSG_TEMPO_ATTESA 'A'
#define MSG_PAROLA 'W'
#define MSG_PUNTI_FINALI 'F'
#define MSG_PUNTI_PAROLA 'P'
#define MSG_SERVER_SHUTDOWN 'B'
#define MSG_POST_BACHECA 'H'
#define MSG_SHOW_BACHECA 'S'

#define MAX_LUNGHEZZA_USERNAME 11 // 10 caratteri + terminatore
#define MIN_LUNGHEZZA_USERNAME 3

#define MIN_LUNGHEZZA_PAROLA 4

char *PAROLIERE = "[PROMPT PAROLIERE] --> ";

typedef struct {
    char type; // è K, E, R, ecc...
    unsigned int length;
    char *data;
} Msg_Socket;

void stampa_comandi();

void comando_non_valido();

int controllo_lunghezza_max (char *argomento, int max_lunghezza);

int controllo_lunghezza_min (char *argomento, int min_lunghezza);

int username_valido(char *nome_utente);

void invio_msg(int fd, Msg_Socket *msg);

Msg_Socket* ricezione_msg(int fd);

