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

#include "macros.h" /*macro per la gestione degli errori sui valori di ritorno*/

#define MAX_CLIENT 32

// funzione per la gestione del server
void server(char* nome_server, int porta_server) {
    /*descrittori dei socket*/
    int fd_server, fd_client;

    int ret;

    struct sockaddr_in server_addr, client_addr;

    socklen_t len_addr;

    // creazione del socket INET restituendo il relativo file descriptor con protocollo TCP
    SYSC(fd_server, socket(AF_INET, SOCK_STREAM, 0), "Errore nella socket");

    // inizializzazione struct server_addr
    memset(&server_addr, 0, sizeof(server_addr));
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(porta_server);
    server_addr.sin_addr.s_addr = inet_addr(nome_server);

    // binding
    SYSC(ret, bind(fd_server, (struct sockaddr *)&server_addr, sizeof(server_addr)), "Errore nella bind");

    // listen
    SYSC(ret, listen(fd_server, MAX_CLIENT), "nella listen");

}


// main per il controllo dei parametri
int main(int argc, char *ARGV[]) {

    char *nome_server;
    int porta_server;

    /*struct per file ????*/
    struct stat info;

    /*controllo dei parametri passati da linea di comando*/

    /*ci devono essere almeno 3 obbligatori: nome dell'eseguibile, nome_server e numero_porta*/
    /* altrimenti errore */
    if (argc < 3) {
        printf("Errore! Parametri: %s nome_server porta_server \n", ARGV[0]);
        exit(EXIT_FAILURE);
    }

    nome_server = ARGV[1];
    porta_server = atoi(ARGV[2]);

    /*controllo dei parametri opzionali: matrici, durata, seed, diz*/

    int opz;
    int op_indx = 0;

    char *data_filename = NULL;
    int durata_in_minuti;
    int rnd_seed;
    char *dizionario = NULL;

    static struct option long_options[] = {
        {"matrici", required_argument, 0, 'm'},
        {"durata", required_argument, 0, 'd'},
        {"seed", required_argument, 0, 's'},
        {"diz", required_argument, 0, 'z'},
        {0, 0, 0, 0}
    };

    while ((opz = getopt_long(argc, ARGV, "m:d:s:z", long_options, &op_indx)) != -1) {
        switch(opz) {
            case 'm':
                data_filename = optarg;
            break;
            case 'd':
                durata_in_minuti = atoi(optarg);
            break;
            case 's':
                rnd_seed = atoi(optarg);
            break;  
            case 'z':
                dizionario = optarg;
            break;
            default:
                printf(stderr, "Errore! Parametri: %s nome_server porta_server [--matrici data_filename] [--durata durata_in_minuti] [--seed rnd_seed] [--diz dizionario] \n", ARGV[0]);
                exit(EXIT_FAILURE);
        }
    }

    // ???????????????????????

     /* controllare se sia matrici e seed sono stati forniti e dare errore */
    if (data_filename != NULL && rnd_seed != 0) {
        fprintf(stderr, "Errore: Non è possibile fornire sia il parametro --matrici che il parametro --seed.\n");
        exit(EXIT_FAILURE);
    }

    /* se sono settati controllali */

    /*controllo nome del file delle matrici*/
    if (data_filename != NULL) {
        /*se è stato fornito, controllare che sia un file regolare*/
        SYSC(ret, stat(data_filename, &info), "Errore nella stat del file delle matrici! \n");
        if (!S_ISREG(info.st_mode)) {
            perror("Attenzione, file non regolare! \n");
            exit(EXIT_FAILURE);
        }
    }

    /*controllo della durata in minuti*/
    if (durata_in_minuti != 0) {
        /*se è stato fornito, controllare che sia un intero maggiore di 0*/
        if (durata_in_minuti <= 0) {
            perror("Attenzione, durata non valida!\n");
            exit(EXIT_FAILURE);
        }
    }

    /*controllo del seed*/
    if (rnd_seed != 0) {
        /*se è stato fornito, controllare che sia un intero maggiore di 0*/
        if (rnd_seed <= 0) {
            perror("Attenzione, seed non valido!\n");
            exit(EXIT_FAILURE);
        }
    }

    /*controllo del file dizionario*/
    if (dizionario != NULL) {
        /*se è stato fornito, controllare che sia un file regolare*/
        SYSC(ret, stat(dizionario, &info), "Errore nella stat del file del dizionario! \n");
        if (!S_ISREG(info.st_mode)) {
            perror("Attenzione, file non regolare! \n");
            exit(EXIT_FAILURE);
        }
    }


}



