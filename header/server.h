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

void caricamento_dizionario(char *file_dizionario, Trie *radice);

void *gioco (void *args);

void *scorer (void *arg);