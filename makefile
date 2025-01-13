CC = gcc
CFLAGS = -Wall -pthread -g

SRC_SERVER = server.c shared.c liste.c trie.c bacheca.c matrice.c
OBJ_SERVER = $(SRC_SERVER:.c=.o)
EXEC_SERVER = server

SRC_CLIENT = client.c shared.c bacheca.c matrice.c
OBJ_CLIENT = $(SRC_CLIENT:.c=.o)
EXEC_CLIENT = client

# parametri obbligatori
NOME = 127.0.0.1
PORTA1 = 1025
PORTA2 = 1026
PORTA3 = 1027

# parametri opzionali
MATRICI ?= --matrici matrix.txt
DURATA ?= --durata 1 # 3
SEED ?= --seed 21
DIZ ?= --diz dictionary_ita.txt

RUN_SERVER1 = ./$(EXEC_SERVER) $(NOME) $(PORTA1)

RUN_SERVER2 = ./$(EXEC_SERVER) $(NOME) $(PORTA1) $(MATRICI) $(DURATA) $(DIZ)

RUN_SERVER3 = ./$(EXEC_SERVER) $(NOME) $(PORTA3) $(MATRICI) $(DURATA) $(SEED) $(DIZ)

RUN_SERVER4 = ./$(EXEC_SERVER) $(NOME) $(PORTA2) $(SEED)

RUN_CLIENT1 = ./$(EXEC_CLIENT) $(NOME) $(PORTA1)

RUN_CLIENT2 = ./$(EXEC_CLIENT) $(NOME) $(PORTA2)


all: $(EXEC_SERVER) $(EXEC_CLIENT)

# collegamento dei file oggetto per creare l'eseguibile per il server
$(EXEC_SERVER): $(OBJ_SERVER)
	$(CC) $(CFLAGS) -o $@ $^

# collegamento dei file oggetto per creare l'eseguibile per il client
$(EXEC_CLIENT): $(OBJ_CLIENT)
	$(CC) $(CFLAGS) -o $@ $^

server.o: server.c
client.o: client.c

# regola generica per compilare separatamente ogni file .c in un file oggetto .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ_SERVER) $(OBJ_CLIENT) $(EXEC_SERVER) $(EXEC_CLIENT)

.PHONY: all clean

# test

test1: 
	$(RUN_SERVER1)

test2:
	$(RUN_SERVER2)

test3:
	$(RUN_SERVER3)

test4:
	$(RUN_CLIENT1)

test5:
	$(RUN_SERVER4)

test6:
	$(RUN_CLIENT2)