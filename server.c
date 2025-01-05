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

#include "header/server.h"

// variabili globali

char *data_filename = NULL;

int durata_gioco = 3;

int rnd_seed;

char *dizionario = NULL;

int durata_pausa = 1;

// strutture dati globali

Trie *radice = NULL;

Messaggio *bacheca;

char **matrice;

lista_thread *Threads;

lista_giocatori *Giocatori;

// mutex



// funzioni

void caricamento_dizionario(char *file_dizionario, Trie *radice) {

    FILE *fp = fopen(file_dizionario, "r");

    if (fp == NULL) {
        perror("Errore nell'apertura del file dizionario");
        exit(EXIT_FAILURE);
    }
    else {
        char tmp_buffer[256];

        while (fscanf(fp, "%s", tmp_buffer) != EOF) {
            inserimento_trie(radice, tmp_buffer);
        }
    }

    fclose(fp);
}

/*int sorting_classifica(const void *a, const void *b) {
    // return strcmp(*(const char **)a, *(const char **)b);
}*/

// funzione per la gestione del client
// fa sia da invio che da ricezione
void *thread_client (void *args) {
    int ret;

    client_args *params = (client_args*)args;
    int fd_c = params -> sck;

    // giocatore associato al thread
    giocatore *player;

    lista_parole *Parole_Trovate = inizializza_parole_trovate();

    // variabile per ricevere il messaggio
    Msg_Socket *richiesta;

    // variabile per inviare il messaggio
    Msg_Socket risposta;

    // prima della registrazione, si accettano solo messaggi di registrazione e di fine
    // (messaggio di aiuto viene gestito nel client)
    while (1) {
        richiesta = ricezione_msg(fd_c);

        risposta.type = ' ';
        risposta.length = 0;
        risposta.data = NULL;

        if (richiesta -> type == MSG_CLIENT_SHUTDOWN) {
            // se il client invia "fine"
            // eliminare il thread dalla lista dei thread
            rimuovi_thread(Threads, pthread_self());
            // deallocare la struct dei suoi parametri
            free(params);
            // terminazione del thread
            pthread_exit(NULL);
        }
        else if (richiesta -> type == MSG_REGISTRA_UTENTE) {
            // controllo lunghezza e caratteri già fatto nel client
            char *nome_utente = richiesta -> data;

            // se la lista giocatori è piena inviare messaggio di errore
            if (Giocatori -> num_giocatori == MAX_CLIENT) {
                char *msg = "Numero massimo di giocatori raggiunto, rimani in attesa per giocare";
                risposta.type = MSG_ERR;
                risposta.length = strlen(msg);

                risposta.data = (char*)malloc(risposta.length + 1);
                strncpy(risposta.data, msg, risposta.length);
                risposta.data[risposta.length] = '\0';

                invio_msg(fd_c, &risposta);
                free(risposta.data);
                continue;
            }
            // se il nome utente è già presente nella lista giocatori inviare messaggio di errore
            else if (cerca_giocatore(Giocatori, nome_utente) == 1) {
                char *msg = "Nome utente già in uso, scegline un altro";
                risposta.type = MSG_ERR;
                risposta.length = strlen(msg);

                risposta.data = (char*)malloc(risposta.length + 1);
                strncpy(risposta.data, msg, risposta.length);
                risposta.data[risposta.length] = '\0';

                invio_msg(fd_c, &risposta);
                free(risposta.data);
                continue;
            }

            // se il nome utente è valido, inizializzare il giocatore
            player = inserisci_giocatore(Giocatori, nome_utente, fd_c);
            player -> parole_trovate = Parole_Trovate;

            // inviare messaggio di avvenuta registrazione
            char *msg = "Registrazione avvenuta con successo, sei pronto a giocare?";
            risposta.type = MSG_OK;
            risposta.length = strlen(msg);

            risposta.data = (char*)malloc(risposta.length + 1);
            strncpy(risposta.data, msg, risposta.length);
            risposta.data[risposta.length] = '\0';

            invio_msg(fd_c, &risposta);
            break;
        }
        else if (richiesta -> data == NULL || richiesta -> type != MSG_REGISTRA_UTENTE || richiesta -> type != MSG_CLIENT_SHUTDOWN) {
            // messaggio non valido
            char *msg = "Messaggio non valido";
            risposta.type = MSG_ERR;
            risposta.length = strlen(msg);

            risposta.data = (char*)malloc(risposta.length + 1);
            strncpy(risposta.data, msg, risposta.length);
            risposta.data[risposta.length] = '\0';

            invio_msg(fd_c, &risposta);
            free(risposta.data);
            continue;
        }

        // pulire le variabili appena utilizzate per lo scambio di messaggi di questa fase
        free(richiesta -> data);
        free(richiesta);

        if (risposta.data != NULL) {
            free(risposta.data);
            risposta.data = NULL;
        }

        risposta.type = ' ';
        risposta.length = 0;
    }

    // invio messaggio di inizio gioco

    // invio della matrice e del tempo


}

// funzione per la gestione del server
void server(char* nome_server, int porta_server) {
    //descrittori dei socket
    int fd_server, fd_client;

    int ret;

    struct sockaddr_in ind_server, ind_client;

    socklen_t len_addr;

    // creazione del socket INET restituendo il relativo file descriptor con protocollo TCP
    SYSC(fd_server, socket(AF_INET, SOCK_STREAM, 0), "Errore nella socket");

    // inizializzazione struct ind_server
    memset(&ind_server, 0, sizeof(ind_server));
    
    ind_server.sin_family = AF_INET;
    ind_server.sin_port = htons(porta_server);
    ind_server.sin_addr.s_addr = inet_addr(nome_server);

    // binding
    SYSC(ret, bind(fd_server, (struct sockaddr *)&ind_server, sizeof(ind_server)), "Errore nella bind");

    // listen
    SYSC(ret, listen(fd_server, MAX_CLIENT), "Errore nella listen");

    inizializza_lista_thread(Threads);
    
    inizializza_lista_giocatori(Giocatori);

    // ciclo di accettazione delle connessioni dei giocatori
    // server continuamente in ascolto
    while(1) {
        // accept
        len_addr = sizeof(ind_client);
        SYSC(fd_client, accept(fd_server, (struct sockaddr*)&ind_client, &len_addr), "Errore nella accept");

        // allocazione della struct per i parametri del thread
        client_args *params = (client_args*)malloc(sizeof(client_args));
        if (params == NULL) {
            perror("Errore nella malloc");
            exit(EXIT_FAILURE);
        }

        // inizializzazione dei parametri
        params -> sck = fd_client;

        // creazione del thread
        SYST(pthread_create(&(params -> t_id), 0, thread_client, params));

        // aggiunta del thread alla lista
        aggiungi_thread(Threads, params -> t_id);
    }
}

// main per il controllo dei parametri
int main(int argc, char *ARGV[]) {

    char *nome_server;
    int porta_server;

    /*struct per file*/
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

    int ret, opz, op_indx = 0;

    int seed_dato = 0;

    static struct option long_options[] = {
        {"matrici", required_argument, 0, 'm'},
        {"durata", required_argument, 0, 'd'},
        {"seed", required_argument, 0, 's'},
        {"diz", required_argument, 0, 'z'},
        {0, 0, 0, 0}
    };

    while ((opz = getopt_long(argc, ARGV, "", long_options, &op_indx)) != -1) {
        switch(opz) {
            case 'm':
                data_filename = optarg;
            break;
            case 'd':
                durata_gioco = atoi(optarg);
            break;
            case 's':
                rnd_seed = atoi(optarg);
                seed_dato = 1;
            break;  
            case 'z':
                dizionario = optarg;
            break;
            default:
                printf("Errore! Parametri: %s nome_server porta_server [--matrici data_filename] [--durata durata_gioco] [--seed rnd_seed] [--diz dizionario] \n", ARGV[0]);
                exit(EXIT_FAILURE);
        }
    }

     /* controllare se sia matrici e seed sono stati forniti e dare errore */
    if (data_filename != NULL && seed_dato == 1) {
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
    if (durata_gioco != 0) {
        /*se è stato fornito, controllare che sia un intero maggiore di 0*/
        if (durata_gioco <= 0) {
            perror("Attenzione, durata non valida!\n");
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

    srand(rnd_seed);

    radice = nuovo_nodo();

    caricamento_dizionario(dizionario, radice);

    server(nome_server, porta_server);

    return 0;
}