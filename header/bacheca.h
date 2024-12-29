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

#define MAX_CARATTERI_MESSAGGIO 128

#define MAX_CARATTERI_USERNAME 11

#define MAX_MESSAGGI 8

// array di stringhe
typedef struct Messaggio {
    char *nome_utente;
    char *messaggio;
} Messaggio;

// allocazione bacheca
Messaggio* allocazione_bacheca();

// inserimento di un messaggio in bacheca
void inserimento_bacheca(Messaggio bacheca [], char *username, char *msg, int *num_msg);

// stampa della bacheca
void stampa_bacheca(Messaggio bacheca[], int *num_msg);

// deallocazione bacheca
void deallocazione_bacheca(Messaggio bacheca[], int *num_msg);