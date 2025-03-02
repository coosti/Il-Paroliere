#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h> 
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h> 
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#define MAX_CARATTERI_MESSAGGIO 128

#define MAX_MESSAGGI 8

// array di stringhe
typedef struct Messaggio {
    char *nome_utente;
    char *messaggio;
} Messaggio;

typedef struct Bacheca {
    Messaggio *messaggi;
    int num_msg;
} Bacheca;

// allocazione bacheca
Bacheca* allocazione_bacheca();

// inserimento di un messaggio in bacheca
void inserimento_bacheca(Bacheca *bacheca, char *username, char *msg);

// stampa della bacheca
void stampa_bacheca(Bacheca *bacheca);

// da bacheca a stringa
char *bacheca_a_stringa(Bacheca *bacheca);

// deallocazione bacheca
void deallocazione_bacheca(Bacheca *bacheca);