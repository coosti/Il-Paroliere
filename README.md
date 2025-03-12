# Il Paroliere 📝

## **Descrizione del progetto**
Il Paroliere è un progetto sviluppato in linguaggio C come conclusione del corso di Laboratorio II della facoltà di Informatica dell'Università di Pisa.
È stato implementato l'omonimo gioco, attraverso un'architettura client-server, in cui ogni giocatore deve trovare il maggior numero di parole ammissibili e componibili su una matrice 4x4, entro un tempo stabilito.

### **Funzionalità**
 - Registrazione e gestione dei giocatori
 - Utilizzo di una struttura dati *trie* per la memorizzazione delle parole del dizionario
 - Utilizzo di un algoritmo di ricerca ricorsivo per le parole nella matrice, basato su un'esplorazione per profondità nelle 8 direzioni (orizzontale, verticale, diagonale)
 - Calcolo del punteggio di ogni parola in base alla lunghezza ed elaborazione della classifica al termine di ogni partita
 - Comunicazione tra giocatori attraverso una bacheca su cui scambiarsi messaggi
 - Comunicazione dei client con il server tramite socket TCP in base ad un protocollo stabilito
 - Gestione multithread

## **Struttura del progetto**
Nel programma sono individuabili due ruoli principali:
- *client*: gestisce l'interazione con l'utente tramite terminale, consentendo di registrarsi, richiedere e ricevere aggiornamenti sullo stato del gioco (visualizzare la matrice di lettere e il tempo di partita/pausa), inviare le parole trovate e pubblicare messaggi sulla bacheca
- *server*: elabora le richieste dei client, gestendoli a partire dalla registrazione fino alla loro chiusura; si occupa di organizzare il gioco e le sue fasi e stilare la classifica relativa al termine di ogni partita

Entrambi comunicano tramite il protocollo definito nei file *shared.h* e *shared.c*, inviando richieste e risposte sul socket.

## **Compilazione ed esecuzione**
È possibile utilizzare le regole di test definite all'interno del makefile.  
Per compilare l'intero progetto:  
`make`  

### Server
Avviare il server specificando indirizzo IP, porta ed eventuali parametri. Per la chiusura premere CTRL-C.

#### avviare il server con IP 127.0.0.1 e porta 1025
`make test1`

#### avviare il server con IP 127.0.0.1, porta 1025 e parametri opzionali per file matrice, file del dizionario e durata del gioco personalizzata 
`make test2`

#### avviare il server con IP 127.0.0.1, porta 1027 e parametri per file matrice, file del dizionario, durata del gioco personalizzata e seed
`make test3`

#### avviare il server con IP 127.0.0.1, porta 1026 e seed
`make test5`

### Client
Avviare il client specificando l'indirizzo IP e la porta a cui collegarsi. Sono ammessi fino a 32 client. Per la chiusura scrivere il comando *fine* oppure premere CTRL-C.

#### avviare il client su IP 127.0.0.1 e porta 1025
`make test4`

#### avviare il client su IP 127.0.0.1 e porta 1026
`make test6`






