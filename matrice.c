#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h> 
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <getopt.h> 
#include <ctype.h>

#include <sys/socket.h> 
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "header/server.h"
#include "header/macros.h"
#include "header/matrice.h"

// variabile globale per memorizzare il valore della posizione corrente nel file
static long pos = 0;

// allocazione matrice
char** allocazione_matrice(){
    char **matrice = (char**)malloc(MAX_CASELLE * sizeof(char*));
    if (matrice ==  NULL) {
        perror("Errore nell'allocazione della matrice");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < MAX_CASELLE; i++) {
        matrice[i] = malloc(MAX_CASELLE * sizeof(char));

        if (matrice[i] == NULL) {
            perror("Errore nell'allocazione della riga della matrice");
            for (int j = 0; j < i; j++) {
                free(matrice[j]);
            }
            free(matrice);
            exit(EXIT_FAILURE);
        }
    }

    return matrice;
}

// inizializzazione con il file fornito
void inizializzazione_matrice(char **matrice, char *file_matrice){
    FILE *fp = fopen(file_matrice, "r");

    if (fp) {
        char buffer[N];

        // mantenere la posizione corrente del file
        fseek(fp, pos, SEEK_SET);

        if (fgets(buffer, sizeof(buffer), fp)) {
            char *c = strtok(buffer, " ");
        
            for (int i = 0; i < MAX_CASELLE; i++) {
                for (int j = 0; j < MAX_CASELLE; j++) {
                    // inserire il token nella casella della matrice
                    // se si trova 'Qu' sostituire con q
                    if (strcmp(c, "Qu") == 0)
                        matrice[i][j] = 'q';
                    else {
                        // assegna il token nella casella, convertendo il carattere in minuscolo
                        matrice[i][j] = tolower(c[0]);
                    }
                    c = strtok(NULL, " ");
                }
            }

            pos = ftell(fp);
        }
    }
    else {
        perror("Errore nell'apertura del file dizionario");
        exit(EXIT_FAILURE);
    }

    fclose(fp);
}

// inizializzazione con lettere casuali
void matrice_casuale(char **matrice){

    for (int i = 0; i < MAX_CASELLE; i++) {
        for (int j = 0; j < MAX_CASELLE; j++) {
            // si genera una lettera minuscola casuale
            char lettera = 'a' + (rand() % 26);

            matrice[i][j] = lettera;
        }
    }
}

// ricerca della parola all'interno della matrice
//int ricerca_matrice(char **matrice, char *parola){


    // quando trovo una parola che ha 'qu' e ho una q nella prossima casella va bene
//}

// stampa della matrice dopo MSG_MATRICE
void stampa_matrice(char **matrice) {
    for(int i = 0; i < MAX_CASELLE; i++) {
        for (int j = 0; j < MAX_CASELLE; j++) {
            if (matrice[i][j] == 'q')
                printf(" %s ", "Qu");
            else
                printf(" %c ", toupper(matrice[i][j]));
        }
        printf("\n");
    }
}

void deallocazione_matrice(char **matrice){
    for (int i = 0; i < MAX_CASELLE; i++) {
        free(matrice[i]);
    }

    free(matrice);
}



