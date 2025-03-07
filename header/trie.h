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

#define MAX_CARATTERI 26

typedef struct Trie {
    struct Trie *figli[MAX_CARATTERI];
    int fine_parola;
} Trie;

Trie* nuovo_nodo();

void inserimento_trie(Trie *radice, char *parola);

int ricerca_trie(Trie *radice, char *parola);

void deallocazione_trie(Trie *radice);