/*
 * evenement.c - Implémentation du système d'événements
 *
 * Queue d'événements avec priorités.
 * Les événements de priorité haute (0) sont traités avant ceux de priorité basse.
 */

#include <stdio.h>
#include <stdlib.h>
#include "evenement.h"

/* -----------------------------------------------------------------------
 * Création et gestion de la queue
 * ----------------------------------------------------------------------- */

QueueEvenement* creer_queue_evenement(int capacite) {
    QueueEvenement *queue = malloc(sizeof(QueueEvenement));
    if (queue == NULL) return NULL;

    queue->events = malloc(capacite * sizeof(Evenement));
    if (queue->events == NULL) {
        free(queue);
        return NULL;
    }

    queue->capacite = capacite;
    queue->nb_events = 0;
    queue->front = 0;

    return queue;
}

/* -----------------------------------------------------------------------
 * Enqueuing : ajouter un événement
 *
 * Insère l'événement à sa place correcte en fonction de la priorité.
 * Les événements de priorité basse sont ajoutés à la fin.
 * Les événements de priorité haute sont insérés avant les événements
 * de priorité plus basse.
 * ----------------------------------------------------------------------- */

void enqueuer_evenement(QueueEvenement *queue, Evenement evt) {
    if (queue->nb_events >= queue->capacite) {
        printf("Queue d'événements pleine !\n");
        return;
    }

    /* Trouver la position d'insertion selon la priorité */
    int pos_insertion = queue->front + queue->nb_events;
    for (int i = queue->front; i < queue->front + queue->nb_events; i++) {
        if (evt.priorite < queue->events[i].priorite) {
            pos_insertion = i;
            break;
        }
    }

    /* Décaler les éléments pour faire de la place */
    for (int i = queue->front + queue->nb_events; i > pos_insertion; i--) {
        queue->events[i] = queue->events[i - 1];
    }

    /* Insérer le nouvel événement */
    queue->events[pos_insertion] = evt;
    queue->nb_events++;
}

/* -----------------------------------------------------------------------
 * Dequeing : extraire le prochain événement (priorité haute en premier)
 * ----------------------------------------------------------------------- */

Evenement dequeur_evenement(QueueEvenement *queue) {
    Evenement evt = queue->events[queue->front];
    queue->front++;
    queue->nb_events--;

    /* Réinitialiser si la queue est vide */
    if (queue->nb_events == 0) {
        queue->front = 0;
    }

    return evt;
}

/* -----------------------------------------------------------------------
 * Vérification de l'état
 * ----------------------------------------------------------------------- */

int queue_vide(QueueEvenement *queue) {
    return queue->nb_events == 0;
}

/* -----------------------------------------------------------------------
 * Libération de la mémoire
 * ----------------------------------------------------------------------- */

void liberer_queue_evenement(QueueEvenement *queue) {
    if (queue == NULL) return;
    free(queue->events);
    free(queue);
}
