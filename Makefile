# Makefile - Compilation du projet SAE 2.3
#
# Commandes :
#   make          → compile le programme
#   make run      → compile et lance avec config2.txt
#   make test1    → teste avec config1.txt (1 switch, 2 stations)
#   make test2    → teste avec config2.txt (3 switchs, 3 stations, cycle)
#   make test3    → teste avec config3.txt (idem mais priorité différente)
#   make clean    → supprime les fichiers compilés

CC     = gcc
CFLAGS = -Wall -Wextra -g -std=c99

# Fichiers sources
SRCS = main.c affichage.c fichier.c ethernet.c stp.c evenement.c
OBJS = $(SRCS:.c=.o)
TARGET = sae

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET) configs/configs/config2.txt

test1: $(TARGET)
	./$(TARGET) configs/configs/config1.txt

test2: $(TARGET)
	./$(TARGET) configs/configs/config2.txt

test3: $(TARGET)
	./$(TARGET) configs/configs/config3.txt

test4: $(TARGET)
	./$(TARGET) configs/configs/config4.txt

clean:
	rm -f $(OBJS) $(TARGET)

# Dépendances
main.o:      main.c types.h affichage.h fichier.h ethernet.h stp.h evenement.h
affichage.o: affichage.c affichage.h types.h
fichier.o:   fichier.c fichier.h types.h
ethernet.o:  ethernet.c ethernet.h types.h affichage.h
stp.o:       stp.c stp.h types.h affichage.h
evenement.o: evenement.c evenement.h types.h
