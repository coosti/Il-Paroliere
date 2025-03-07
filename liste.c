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

// LISTA DI THREAD ATTIVI

void inizializza_lista_thread (lista_thread **lista) {
    *lista = (lista_thread*)malloc(sizeof(lista_thread));
    if (*lista == NULL) {
        perror("Errore nell'allocazione della lista di thread attivi");
        exit(EXIT_FAILURE);
    }

    (*lista) -> head = NULL;
    (*lista) -> num_thread = 0;

    (*lista) -> num_thread = 0;
}

// inserimento in testa di un nuovo thread
void inserisci_thread (lista_thread *lista, pthread_t tid, int fd) {

    // creazione dell'elemento nella lista per il nuovo thread
    thread_attivo *thd;
    SYSCN(thd, (thread_attivo*)malloc(sizeof(thread_attivo)), "Errore nell'allocazione del thread attivo");

    thd -> t_id = tid;
    thd -> fd_c = fd;
    thd -> next = NULL;

    // inserimento in testa
    // caso lista vuota
    if (lista -> head == NULL) {
        lista -> head = thd;
    }
    else {
        // il next del nuovo thread è la vecchia testa della lista  
        thd -> next = lista -> head;

        // la nuova testa è il nuovo thread
        lista -> head = thd;
    }

    // incremento del numero di thread attivi
    lista -> num_thread++;
}

// rimozione dalla lista del thread con tid passato per argomento
void rimuovi_thread (lista_thread *lista, pthread_t tid) {
    
    // puntatore per scorrere
    thread_attivo *tmp;
    // puntatore per mantenere l'elemento precedente a quello corrente
    thread_attivo *prev = NULL;

    // caso lista vuota
    if (lista -> head == NULL) {
        printf("Lista vuota\n");
        return;
    }

    tmp = lista -> head;

    // se il thread da rimuovere è in testa
    if (tmp != NULL && pthread_equal(tmp -> t_id, tid)) {
        // la nuova testa è l'elemento successivo di tmp
        lista -> head = tmp -> next;
        tmp -> next = NULL;

        // deallocare la memoria del thread
        free(tmp);
        return;
    }
    else {
        // se è nel mezzo, scorrere la lista
        int i = 0;

        while (tmp != NULL) {
            if (pthread_equal(tmp -> t_id, tid)) {
                // il next dell'elemento precedente punta all'elemento successivo di tmp
                prev -> next = tmp -> next;
                // mettere a null il next di tmp
                tmp -> next = NULL;

                free(tmp);
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

// funzione per il recupero del file descriptor associato al tid passato per argomento
int recupera_fd_thread (lista_thread *lista, pthread_t tid) {
    // scorrimento della lista dei thread
    thread_attivo *tmp = lista -> head;

    if (tmp == NULL) {
        return -1;
    }

    while (tmp != NULL) {
        if (pthread_equal(tmp -> t_id, tid)) {
            return tmp -> fd_c;
        }
        tmp = tmp -> next;
    }

    return -1;
}

// pulizia della lista di thread
void svuota_lista_thread (lista_thread *lista) {
    
    // due puntatori per scorrere la lista e svuotarla
    thread_attivo *tmp = lista -> head;
    thread_attivo *nxt = NULL;

    while (tmp != NULL) {
        nxt = tmp -> next;

        // deallocare memoria
        free(tmp);

        // decrementare il numero di thread attivi
        lista -> num_thread--;

        tmp = nxt;
    }

    lista -> head = NULL;
    lista -> num_thread = 0;
}

// invio del segnale passato come argomento a tutti i thread attivi della lista
void invia_sigusr (lista_thread *lista, int segnale) {
    // scorrimento della lista di thread
    thread_attivo *tmp = lista -> head;

    while (tmp != NULL) {
        // invio del segnale
        SYST(pthread_kill(tmp -> t_id, segnale));

        printf("segnale inviato \n");

        tmp = tmp -> next;
    }
}

// LISTA DEI GIOCATORI

void inizializza_lista_giocatori (lista_giocatori **lista) {
    *lista = (lista_giocatori*)malloc(sizeof(lista_giocatori));
    if (*lista == NULL) {
        perror("Errore nell'allocazione della lista di giocatori");
        exit(EXIT_FAILURE);
    }

    (*lista) -> head = NULL;
    (*lista) -> num_giocatori = 0;

    (*lista) -> num_giocatori = 0;
}

// inserimento in testa di un nuovo giocatore associato al thread che ha invocato la funzione
giocatore *inserisci_giocatore (lista_giocatori *lista, char *nome_utente, int fd) {
    // creazione dell'elemento giocatore
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

    // inizializzazione della lista delle parole trovate a NULL
    g -> parole_trovate = inizializza_parole();

    // assegnazione del descrittore del socket
    g -> fd_c = fd;

    g -> next = NULL;

    if (lista -> head == NULL) {
        lista -> head = g;
    }
    else {
        g -> next = lista -> head;

        lista -> head = g;
    }

    return g;
}

// ricerca con username di un giocatore nella lista
int cerca_giocatore (lista_giocatori *lista, char *nome_utente) {
    giocatore *tmp = lista -> head;
    // caso lista vuota
    if (tmp == NULL) {
        return 0;
    }

    while (tmp != NULL) {
        // confronto tra nome utente corrente in tmp e il nome utente passato come parametro
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

// recupero del nome utente associato al tid passato per argomento
char *recupera_username (lista_giocatori *lista, pthread_t tid) {
    // scorrimento della lista
    giocatore *tmp = lista -> head;

    if (tmp == NULL) {
        return NULL;
    }

    while (tmp != NULL) {
        // confronto tra il tid corrente e il tid passato
        if (pthread_equal(tmp -> t_id, tid)) {
            return tmp -> username;
        }
        tmp = tmp -> next;        
    }

    return NULL;
}

// recupero del punteggio del giocatore associato al tid passato come argomento
int recupera_punteggio (lista_giocatori *lista, pthread_t tid) {
    giocatore *tmp = lista -> head;
    
    if (tmp == NULL) {
        return -1;
    }

    while (tmp != NULL) {
        if (pthread_equal(tmp -> t_id, tid)) {
            return tmp -> punteggio;
        }
        tmp = tmp -> next;
    }

    return -1;
}

// recupero del file descriptor del giocatore associato al tid passato come argomento
int recupera_fd_giocatore (lista_giocatori *lista, pthread_t tid) {
    giocatore *tmp = lista -> head;

    if (tmp == NULL) {
        return -1;
    }

    while (tmp != NULL) {
        if (pthread_equal(tmp -> t_id, tid)) {
            return tmp -> fd_c;
        }
        tmp = tmp -> next;
    }

    return -1;
}

// modifica a 0 del punteggio del giocatore associato al tid passato come argomento
void resetta_punteggio (lista_giocatori *lista, pthread_t tid) {
    giocatore *tmp = lista -> head;

    if (tmp == NULL) {
        return;
    }

    while (tmp != NULL) {
        if (pthread_equal(tmp -> t_id, tid)) {
            tmp -> punteggio = 0;
            return;
        }
        tmp = tmp -> next;
    }
}

// rimozione dalla lista del giocatore con username passato come argomento
void rimuovi_giocatore (lista_giocatori *lista, char *nome_utente) {

    // scorrimento della lista
    giocatore *tmp = lista -> head;
    giocatore *prev = NULL;

    // caso lista vuota
    if (tmp == NULL) {
        printf("Lista vuota\n");
        return;
    }

    // giocatore da eliminare in testa
    // confronto tra stringhe del nome utente di tmp e il nome utente passato come parametro
    if (strcmp(tmp ->username, nome_utente) == 0) {
        lista -> head = tmp -> next;
        tmp -> next = NULL;

        // deallocare la memoria
        free(tmp -> username);
        free(tmp);

        return;
    }
    
    // giocatore da eliminare nel mezzo
    while (tmp != NULL) {
        if (strcmp(tmp ->username, nome_utente) == 0) {
            // il next dell'elemento precedente punta all'elemento successivo di tmp
            prev -> next = tmp -> next;
            tmp -> next = NULL;

            free(tmp -> username);
            free(tmp);

            return;
        }

            prev = tmp;
            tmp = tmp -> next;
    }

}

// pulizia della lista dei giocatori
void svuota_lista_giocatori (lista_giocatori *lista) {
    if (lista -> head == NULL) {
        return;
    }

    giocatore *tmp = lista -> head;
    giocatore *nxt = NULL;

    while (tmp != NULL) {
        nxt = tmp -> next;

        // liberare memoria nome utente
        free(tmp -> username);

        // liberare memoria giocatore
        free(tmp);

        lista -> num_giocatori--;

        tmp = nxt;
    }

    lista -> head = NULL;
    lista -> num_giocatori = 0;
}

// LISTA DI PAROLE TROVATE per giocatore corrente

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

// inserimento in testa di una nuova parola indovinata dal giocatore con relativo punteggio
void inserisci_parola (lista_parole *lista, char *parola, int punti) {
    parola_trovata *p;
    SYSCN(p, (parola_trovata*)malloc(sizeof(parola_trovata)), "Errore nell'allocazione della parola trovata");

    // inizializzazione della parola
    p -> parola = (char*)malloc(strlen(parola) + 1);
    if (p -> parola == NULL) {
        perror("Errore nell'allocazione della parola");
        free(p);
        exit(EXIT_FAILURE);
    }
    strcpy(p -> parola, parola);

    // inserimento del punteggio
    p -> punti = punti;

    p -> next = NULL;

    // inserimento in testa
    if (lista -> head == NULL) {
        lista -> head = p;
    }
    else {
        // il puntatore a next della nuova parola è la vecchia testa della lista
        p -> next = lista -> head;

        // la nuova testa è la nuova parola
        lista -> head = p;
    }

    // incremento del numero di parole trovate dal giocatore
    lista -> num_parole++;
}

// ricerca nella lista della parola passata come argomento
int cerca_parola (lista_parole *lista, char *parola) {
    parola_trovata *tmp = lista -> head;

    // caso lista vuota
    if (tmp == NULL) {
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

    // se non viene trovata ok
    return 0;
}

// pulizia della lista di parole per la partita successiva
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

    free(lista);
}

// CODA DEI RISULTATI

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

// inserimento in coda del risultato, con username e suo punteggio
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

    // se la coda è vuota
    if (coda -> head == NULL) {
        // il nuovo elemento è sia il riferimento in testa che in coda
        coda -> head = r;
        coda -> tail = r;
    }
    // se la coda non è vuota
    else {
        // si aggiorna il riferimento next della coda con il nuovo elemento
        coda -> tail -> next = r;
        coda -> tail = r; 
    }
}

// lettura del risultato con rimozione in testa
risultato *leggi_risultato (coda_risultati *coda) {
    // controllare che non sia vuota
    if (coda -> head == NULL) {
        return NULL;
    }

    risultato *tmp = coda -> head;

    // se l'elemento da togliere è l'unico della coda
    if (coda -> head == coda -> tail) {
        // mettere a null entrambi i riferimenti di testa e coda
        coda -> head = NULL;
        coda -> tail = NULL;
    }
    // se ci sono altri elementi oltre a quello tolto
    else {
        // la nuova testa è l'elemento successivo
        coda -> head = coda -> head -> next;        
    }
    
    // pulire il riferimento next del nodo tolto
    tmp -> next = NULL;

    return tmp;
}

// funzione di stampa per la coda dei risultati (per debug)
void stampa_coda_risultati(coda_risultati *coda) {
    risultato *current = coda->head;
    printf("Numero di risultati: %d\n", coda->num_risultati);
    while (current != NULL) {
        printf("Username: %s, Punteggio: %d\n", current->username, current->punteggio);
        current = current->next;
    }
}

// pulizia della coda risultati per la prossima partita
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