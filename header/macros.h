#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

// macro per il controllo delle system call sui processi che ritornano -1
#define SYSC(v, c, m) \
    if ((v = c) == -1) { \
        perror(m); \
        exit(errno); \
    }

// macro per il controllo delle chiamate che ritornano NULL
#define SYSCN(v, c, m) \
    if ((v = c) == NULL) { \
        perror(m); \
        exit(errno); \
    }

// macro per il controllo delle system call sui thread che ritornano dei valori diversi da 0 in caso di errore
#define SYST(cmd) \
    if (cmd != 0) { \
        perror("errore thread"); \
        exit(EXIT_FAILURE); \
    }