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

// inizializzazione con il file fornito
void inizializzazione_matrice(char **matrice, char *file_matrice){
    FILE *fp = fopen(file_matrice, "r");

    int i = 0;

    if (fp) {
        char buffer[N];

        // mantenere la posizione corrente del file
        fseek(fp, pos, SEEK_SET);

        if (fgets(buffer, sizeof(buffer), fp)) {
            char *c = strtok(buffer, " ");
        
            for (i = 0; i < MAX_CASELLE; i++) {
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

        // quando si arriva alla fine del file, inizio a generare matrici casuali
        if (i < MAX_CASELLE) {
            matrice_casuale(matrice);
        }

        fclose(fp);
    }
    else {
        perror("Errore nell'apertura del file dizionario");
        exit(EXIT_FAILURE);
    }
}

// funzione ausiliaria ricorsiva
int ricerca_parola (char **matrice, char *parola, int i_riga, int j_colonna, int r[], int c[], int p, int length, int visitata[MAX_CASELLE][MAX_CASELLE]) {
    
    // casi base

    // se l'indice corrisponde con la lunghezza della parola -> trovata
    if (p == length) {
        return 1;
    }

    // controllare che sia una casella valida
    if (i_riga < 0 || i_riga >= MAX_CASELLE || j_colonna < 0 || j_colonna >= MAX_CASELLE) {
        return 0;
    }

    // se la casella è già stata visitata -> non va bene!!
    if (visitata[i_riga][j_colonna] == 1) {
        return 0;
    }

    // se la lettera della casella non corrisponde al carattere della parola
    if (matrice[i_riga][j_colonna] != parola[p]) {
        return 0;
    }

    // marcare la casella appena visitata
    visitata[i_riga][j_colonna] = 1;

    // caso ricorsivo

    // visitare i cammini sulle 8 direzioni
    for (int i = 0; i < 8; i++) {

        // coordinate in cui cercare
        int x = i_riga + r[i];
        int y = j_colonna + c[i];

        // se la ricerca in quella direzione ha avuto successo -> trovata
        if (ricerca_parola(matrice, parola, x, y, r, c, p + 1, length, visitata)) {
            return 1;
        }
    }

    // 'pulire' la casella visitata
    visitata[i_riga][j_colonna] = 0;

    // -> non trovata
    return 0;
}

// ricerca della parola all'interno della matrice
int ricerca_matrice(char **matrice, char *parola){
    // lunghezza della parola
    int length = strlen(parola);

    // inizializzare una matrice di 0 e 1 per marcare le celle già visitate durante il cammino
    int visitata[MAX_CASELLE][MAX_CASELLE] = {0};

    // definire le direzioni con coordinate (movimenti in ordine orario)
    int r[] = {-1, -1, 0, 1, 1, 1, 0, -1};
    int c[] = {0, 1, 1, 1, 0, -1, -1, -1};

    // scorrere ogni casella della matrice 
    for (int i = 0; i < MAX_CASELLE; i++) {
        for (int j = 0; j < MAX_CASELLE; j++) {
            // se la lettera in quella casella è uguale all'iniziale della parola
            if (matrice[i][j] == parola[0]) {
                // faccio partire la ricerca da quella casella
                if (ricerca_parola(matrice, parola, i, j, r, c, 0, length, visitata)) {
                    // parola trovata
                    return 1;
                }
            }
        }
    }

    // parola non trovata
    return 0;   
}

// stampa della matrice
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

// stampa della matrice da MSG_MATRICE
void stampa_matrice_stringa (char *matrice) {
    int k = 0;

    for (int i = 0; i < MAX_CASELLE; i++) {
        for (int j = 0; j < MAX_CASELLE; j++) {
            // togliere gli spazi dalla stringa
            while (matrice[k] == ' ') {
                k++;
            }

            // se si trova q, sostituire con Qu
            if (matrice[k] == 'q') {
                printf(" %s ", "Qu");
            }
            else {
                printf(" %c ", toupper(matrice[k]));
            }

            k++;
        }
        printf("\n");
    }
}

char *matrice_a_stringa(char **matrice, char *stringa) {
    int k = 0;

    for (int i = 0; i < MAX_CASELLE; i++) {
        for (int j = 0; j < MAX_CASELLE; j++) {
            stringa[k++] = matrice[i][j];
            stringa[k++] = ' ';
        }
    }
    stringa[k - 1] = '\0';

    return stringa;
}

void deallocazione_matrice(char **matrice){
    for (int i = 0; i < MAX_CASELLE; i++) {
        free(matrice[i]);
    }

    free(matrice);
}