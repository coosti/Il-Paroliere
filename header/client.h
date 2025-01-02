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
#include "header/shared.h"
#include "header/bacheca.h"
#include "header/matrice.h"

#define NUM_THREAD 2

#define MAX_LUNGHEZZA_STDIN 256

// struct per raggruppare i parametri utili dei thread
typedef struct {
    pthread_t t_id; // tid
    int *sck;   // puntatore al file descriptor del socket
} thread_args;

void *invio_client (void *args);

void *ricezione_client (void *args);
