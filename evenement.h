/*
 * evenement.h - Système d'événements pour la simulation
 *
 * Définit une architecture événementielle où chaque étape est un événement.
 * Les événements sont traités par ordre de priorité dans une queue.
 */

#ifndef EVENEMENT_H
#define EVENEMENT_H

#include "types.h"

/* -----------------------------------------------------------------------
 * Types d'événements
 * ----------------------------------------------------------------------- */

typedef enum {
    EVENT_DEMO_ETAPE1,           /* Affichage des structures */
    EVENT_DEMO_ETAPE2,           /* Chargement depuis fichier */
    EVENT_DEMO_ETAPE3,           /* Diffusion Ethernet */
    EVENT_DEMO_ETAPE4,           /* Protocole STP */
    EVENT_QUIT                   /* Signal d'arrêt */
} TypeEvenement;

/* -----------------------------------------------------------------------
 * Données associées à un événement
 * ----------------------------------------------------------------------- */

typedef struct {
    const char *fichier;         /* Fichier de config pour les étapes 2, 3, 4 */
} DonneeEvenement;

/* -----------------------------------------------------------------------
 * Structure d'un événement
 * ----------------------------------------------------------------------- */

typedef struct {
    TypeEvenement type;          /* Type de l'événement */
    DonneeEvenement donnees;     /* Données spécifiques */
    int priorite;                /* Priorité d'exécution (0 = haute) */
} Evenement;

/* -----------------------------------------------------------------------
 * Queue d'événements (file FIFO)
 * ----------------------------------------------------------------------- */

typedef struct {
    Evenement *events;           /* Tableau d'événements */
    int capacite;                /* Taille du tableau */
    int nb_events;               /* Nombre d'événements actuellement en queue */
    int front;                   /* Indice de la tête */
} QueueEvenement;

/* Crée une nouvelle queue d'événements */
QueueEvenement* creer_queue_evenement(int capacite);

/* Ajoute un événement à la queue */
void enqueuer_evenement(QueueEvenement *queue, Evenement evt);

/* Extrait le prochain événement de la queue (par priorité) */
Evenement dequeur_evenement(QueueEvenement *queue);

/* Vérifie si la queue est vide */
int queue_vide(QueueEvenement *queue);

/* Libère la mémoire de la queue */
void liberer_queue_evenement(QueueEvenement *queue);

#endif
