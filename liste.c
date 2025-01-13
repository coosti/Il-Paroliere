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
#include "header/liste.h"

// funzioni per la lista di thread

void inizializza_lista_thread (lista_thread **lista) {
    *lista = (lista_thread*)malloc(sizeof(lista_thread));
    if (lista == NULL) {
        perror("Errore nell'allocazione della lista di thread attivi");
        exit(EXIT_FAILURE);
    }

    (*lista) -> head = NULL;
    (*lista) -> num_thread = 0;
}

void inserisci_thread (lista_thread *lista, pthread_t tid) {

    // creazione del'elemento nella lista per il nuovo thread
    thread_attivo *thd = (thread_attivo*)malloc(sizeof(thread_attivo));
    if (thd == NULL) {
        perror("Errore nell'allocazione del thread attivo");
        exit(EXIT_FAILURE);
    }

    thd -> t_id = tid;

    // inserimento in testa
    // il next del nuovo thread è la vecchia testa della lista
    thd -> next = lista -> head;

    // la nuova testa è il nuovo thread
    lista -> head = thd;

    // incremento del numero di thread attivi
    lista -> num_thread++;
}

void rimuovi_thread (lista_thread *lista, pthread_t tid) {
    
    // puntatore per scorrere
    thread_attivo *tmp = lista -> head;
    // puntatore per mantenere l'elemento precedente a quello corrente
    thread_attivo *prev = NULL;

    if (tmp == NULL) {
        printf("Lista vuota\n");
        return;
    }

    // se il thread da rimuovere è in testa
    if (tmp != NULL && tmp -> t_id == tid) {
        // la nuova testa è l'elemento successivo di tmp
        lista -> head = tmp -> next;
        tmp -> next = NULL;

        // deallocare la memoria del thread
        free(tmp);
        // decrementare il numero di thread
        lista -> num_thread--;
        return;
    }
    else {
        // se è nel mezzo, scorrere la lista
        int i = 0;

        while (tmp != NULL && i < lista -> num_thread) {
            if (tmp -> t_id == tid) {
                // il next dell'elemento precedente punta all'elemento successivo di tmp
                prev -> next = tmp -> next;
                // mettere a null il next di tmp
                tmp -> next = NULL;

                free(tmp);
                lista -> num_thread--;
                return;
            }

            // per andare avanti prev punta l'elemento corrente tmp
            prev = tmp;
            // tmp è ciò che puntava il suo next
            tmp = tmp -> next;

            i++;
        }
        return;
    }
}

void svuota_lista_thread (lista_thread *lista) {
    
    // due puntatori per scorrere la lista e svuotarla
    thread_attivo *tmp = lista -> head;
    thread_attivo *nxt = NULL;

    while (tmp != NULL) {
        nxt = tmp -> next;

        // deallocare memoria e mano a mano diminuire il numero di thread
        free(tmp);
        lista -> num_thread--;

        tmp = nxt;
    }

    lista -> head = NULL;
    lista -> num_thread = 0;
}

void invia_sigusr (lista_thread *lista, int segnale) {
    thread_attivo *tmp = lista -> head;

    while (tmp != NULL) {
        SYST(pthread_kill(tmp -> t_id, segnale));

        printf("segnale inviato \n");

        tmp = tmp -> next;
    }
}

void inizializza_lista_giocatori (lista_giocatori **lista) {
    *lista = (lista_giocatori*)malloc(sizeof(lista_giocatori));
    if (lista == NULL) {
        perror("Errore nell'allocazione della lista di giocatori");
        exit(EXIT_FAILURE);
    }

    (*lista) -> head = NULL;
    (*lista) -> num_giocatori = 0;
}

giocatore *inserisci_giocatore (lista_giocatori *lista, char *nome_utente, int fd) {

    // creazione dell'elemento del giocatore nella lista
    giocatore *g;
    SYSCN(g, (giocatore*)malloc(sizeof(giocatore)), "Errore nell'allocazione del giocatore");

    memset(g, 0, sizeof(giocatore));

    // assegnazione del nome utente passato come parametro
    g -> username = (char*)malloc(strlen(nome_utente) + 1);
    if (g -> username == NULL) {
        perror("Errore nell'allocazione del nome utente");
        free(g);
        exit(EXIT_FAILURE);
    }
    strcpy(g -> username, nome_utente);

    // assegnazione del tid del thread
    g -> t_id = pthread_self();

    // assegnazione del tid del thread handler
    g -> tid_sigclient = -1;

    // inizializzazione del punteggio a 0 a inizio partita
    g -> punteggio = 0;

    // inizializzazione della lista delle parole trovate a NULL
    g -> parole_trovate = NULL;

    // assegnazione del descrittore del socket
    g -> fd_c = fd;

    g -> next = lista -> head;

    lista -> head = g;

    return g;
}

int cerca_giocatore (lista_giocatori *lista, char *nome_utente) {
    giocatore *tmp = lista -> head;

    if (tmp == NULL) {
        return 0;
    }

    while (tmp != NULL) {
        // confronto tra nome utente di tmp e il nome utente passato come parametro
        if (strcmp(tmp ->username, nome_utente) == 0) {
            // esiste già un utente con questo username
            return 1;
        }
        
        // altrimenti andare avanti
        tmp = tmp -> next;
    }

    // se non viene trovato ok
    return 0;
}

char *recupera_username (lista_giocatori *lista, pthread_t tid) {
    giocatore *tmp = lista -> head;

    if (tmp == NULL) {
        return NULL;
    }

    while (tmp != NULL) {
        if (tmp -> t_id == tid) {
            return tmp -> username;
        }
        tmp = tmp -> next;        
    }

    return NULL;
}

int recupera_punteggio (lista_giocatori *lista, pthread_t tid) {
    giocatore *tmp = lista -> head;
    
    if (tmp == NULL) {
        return -1;
    }

    while (tmp != NULL) {
        if (tmp -> t_id == tid) {
            return tmp -> punteggio;
        }
        tmp = tmp -> next;
    }

    return -1;
}

int recupera_fd (lista_giocatori *lista, pthread_t tid) {
    giocatore *tmp = lista -> head;

    if (tmp == NULL) {
        return -1;
    }

    while (tmp != NULL) {
        if (tmp -> t_id == tid) {
            return tmp -> fd_c;
        }
        tmp = tmp -> next;
    }

    return -1;
}

void resetta_punteggio (lista_giocatori *lista, pthread_t tid) {
    giocatore *tmp = lista -> head;

    if (tmp == NULL) {
        return;
    }

    while (tmp != NULL) {
        if (tmp -> tid_sigclient == tid) {
            tmp -> punteggio = 0;
            return;
        }
        tmp = tmp -> next;
    }
}

void rimuovi_giocatore (lista_giocatori *lista, char *nome_utente) {

    giocatore *tmp = lista -> head;
    giocatore *prev = NULL;

    if (tmp == NULL) {
        printf("Lista vuota\n");
        return;
    }

    // giocatore da eliminare è in testa
    // confronto tra stringhe del nome utente di tmp e il nome utente passato come parametro
    if (tmp != NULL && strcmp(tmp ->username, nome_utente)) {
        lista -> head = tmp -> next;
        tmp -> next = NULL;

        // deallocare la memoria dell'username e del giocatore stesso
        free(tmp -> username);
        free(tmp);

        // decrementare il numero di giocatori
        lista -> num_giocatori--;
        return;
    }
    else {
        // giocatore da eliminare è nel mezzo
        int i = 0;

        while (tmp != NULL && i < lista -> num_giocatori) {
            if (strcmp(tmp ->username, nome_utente)) {
                prev -> next = tmp -> next;
                tmp -> next = NULL;

                free(tmp -> username);
                free(tmp);

                lista -> num_giocatori--;
                return;
            }

            prev = tmp;
            tmp = tmp -> next;
            i++;
        }

        return;
    }
}

void svuota_lista_giocatori (lista_giocatori *lista) {
    giocatore *tmp = lista -> head;
    giocatore *nxt = NULL;

    while (tmp != NULL) {
        nxt = tmp -> next;

        free(tmp -> username);
        free(tmp);

        lista -> num_giocatori--;

        tmp = nxt;
    }

    lista -> head = NULL;
    lista -> num_giocatori = 0;
}


lista_parole *inizializza_parole () {
    lista_parole *lista = (lista_parole*)malloc(sizeof(lista_parole));
    if (lista == NULL) {
        perror("Errore nell'allocazione della lista di parole");
        exit(EXIT_FAILURE);
    }

    lista -> head = NULL;
    lista -> num_parole = 0;

    return lista;
}

void inserisci_parola (lista_parole *lista, char *parola, int punti) {
    // creazione del'elemento nella lista per il nuovo thread
    parola_trovata *p = (parola_trovata*)malloc(sizeof(parola_trovata));
    if (p == NULL) {
        perror("Errore nell'allocazione del thread attivo");
        exit(EXIT_FAILURE);
    }

    // inizializzazione della parola trovata
    p -> parola = (char*)malloc(strlen(parola) + 1);
    if (p -> parola == NULL) {
        perror("Errore nell'allocazione della parola");
        free(p);
        exit(EXIT_FAILURE);
    }
    strncpy(p -> parola, parola, strlen(parola));
    p -> parola[strlen(parola)] = '\0';

    p -> punti = punti;

    // inserimento in testa
    // il puntatore a next della nuova parola è la vecchia testa della lista
    p -> next = lista -> head;

    // la nuova testa è la nuova parola
    lista -> head = p;

    // incremento del numero di parole trovate dal giocatore
    lista -> num_parole++;
}

int cerca_parola (lista_parole *lista, char *parola) {
    parola_trovata *tmp = lista -> head;

    if (tmp == NULL) {
        // se la lista è vuota (improbabile ma non si sa mai)
        return 0;
    }

    while (tmp != NULL) {
        // confronto tra la parola passata come argomento e la parola puntata da tmp
        if (strcmp(tmp -> parola, parola) == 0) {
            return 1;
        }
        
        // altrimenti si va avanti
        tmp = tmp -> next;
    }

    // se non viene trovata
    return 0;
}

void svuota_lista_parole (lista_parole *lista) {
    parola_trovata *tmp = lista -> head;
    parola_trovata *nxt = NULL;

    while (tmp != NULL) {
        nxt = tmp -> next;

        free(tmp -> parola);
        free(tmp);

        lista -> num_parole--;

        tmp = nxt;
    }

    lista -> head = NULL;
    lista -> num_parole = 0;
}

void inizializza_coda_risultati (coda_risultati **coda) {
    *coda = (coda_risultati*)malloc(sizeof(coda_risultati));
    if (*coda == NULL) {
        perror("Errore nell'allocazione della coda dei risultati");
        exit(EXIT_FAILURE);
    }

    (*coda) -> head = NULL;
    (*coda) -> tail = NULL;

    (*coda) -> num_risultati = 0;
}

// inserimento in coda
void inserisci_risultato (coda_risultati *coda, char *username, int punteggio) {
    risultato *r = (risultato*)malloc(sizeof(risultato));
    if (r == NULL) {
        perror("Errore nell'allocazione del risultato");
        exit(EXIT_FAILURE);
    }

    size_t len = strlen(username);

    // inizializzazione risultato
    r -> username = (char*)malloc((len + 1) * sizeof(char));
    if (r -> username == NULL) {
        perror("Errore nell'allocazione del nome utente");
        free(r);
        exit(EXIT_FAILURE);
    }
    strncpy(r -> username, username, len);
    r -> username[len] = '\0';

    r -> punteggio = punteggio;

    r -> next = NULL;

    // inserimento in coda
    // se la coda è vuota il nuovo elemento è sia testa che coda
    if (coda -> head == NULL) {
        coda -> head = r;
        coda -> tail = r;
    }
    else {
        // se la coda non è vuota, si aggiorna il riferimento next della coda con il nuovo elemento
        coda -> tail -> next = r;
        coda -> tail = r; 
    }
}

// rimozione in testa
risultato *leggi_risultato (coda_risultati *coda) {
    // controllare che non sia vuota
    if (coda -> head == NULL) {
        return NULL;
    }

    // puntatore per mantenere il riferimento all'elemento da restituire
    risultato *tmp = coda -> head;

    // se l'elemento da togliere è l'unico della coda
    if (coda -> head == coda -> tail) {
        // mettere a null entrambi i riferimenti di testa e coda
        coda -> head = NULL;
        coda -> tail = NULL;
    }
    else {
        // se ci sono altri elementi oltre a quello tolto
        // la nuova testa è l'elemento successivo
        coda -> head = coda -> head -> next;        
    }
    
    // pulire il riferimento next del nodo tolto
    tmp -> next = NULL;

    return tmp;
}

void stampa_coda_risultati(coda_risultati *coda) {
    risultato *current = coda->head;
    printf("Numero di risultati: %d\n", coda->num_risultati);
    while (current != NULL) {
        printf("Username: %s, Punteggio: %d\n", current->username, current->punteggio);
        current = current->next;
    }
}

void svuota_coda_risultati (coda_risultati *coda) {
    risultato *tmp = coda -> head;
    risultato *nxt = NULL;

    while (tmp != NULL) {
        nxt = tmp -> next;

        free(tmp -> username);
        free(tmp);

        tmp = nxt;
    }

    coda -> head = NULL;
    coda -> tail = NULL;
}