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

#define MAX_CASELLE 4

static long pos;

char** allocazione_matrice();

void inizializzazione_matrice(char **matrice, char *file_matrice);

void matrice_casuale(char **matrice);

int ricerca_matrice(char **matrice, char *parola);

void stampa_matrice(char **matrice);

void deallocazione_matrice(char **matrice);

