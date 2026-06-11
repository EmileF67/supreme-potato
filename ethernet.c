/*
 * ethernet.c - Trame Ethernet et diffusion dans le réseau
 *
 * Ce fichier implémente :
 *   - La création d'une trame Ethernet (structure conforme à la norme)
 *   - La simulation de la commutation de trames dans un réseau local
 *
 * Algorithme de commutation (simplifié) :
 *   Quand un switch reçoit une trame sur un port P :
 *     1. Il mémorise que l'adresse MAC source est accessible via P
 *        (mise à jour de sa table de commutation)
 *     2. Il regarde si l'adresse MAC destination est dans sa table :
 *        - OUI → il envoie la trame uniquement sur ce port
 *        - NON → il envoie la trame sur TOUS ses ports actifs (sauf P)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ethernet.h"
#include "affichage.h"

/* -----------------------------------------------------------------------
 * Comparaison d'adresses MAC
 * ----------------------------------------------------------------------- */

/* Retourne 1 si les deux adresses MAC sont identiques */
static int mac_egales(MAC a, MAC b) {
    return memcmp(a.octets, b.octets, 6) == 0;
}

/* Retourne 1 si l'adresse MAC est celle de broadcast (FF:FF:FF:FF:FF:FF) */
static int est_broadcast(MAC mac) {
    for (int i = 0; i < 6; i++)
        if (mac.octets[i] != 0xFF) return 0;
    return 1;
}

/* -----------------------------------------------------------------------
 * Création d'une trame Ethernet
 * ----------------------------------------------------------------------- */

TrameEthernet creer_trame(MAC source, MAC destination, uint16_t type,
                          uint8_t *donnees, int longueur) {
    TrameEthernet t;

    /* Préambule : 7 octets valant 0xAA (10101010 en binaire) */
    for (int i = 0; i < 7; i++)
        t.preambule[i] = 0xAA;

    /* SFD (Start Frame Delimiter) : 0xAB (10101011 en binaire) */
    t.sfd = 0xAB;

    t.destination = destination;
    t.source      = source;
    t.type        = type;

    /* Copie des données (jusqu'à 1500 octets) */
    if (longueur > 1500) longueur = 1500;
    memcpy(t.donnees, donnees, longueur);
    t.longueur_donnees = longueur;

    /* Bourrage (padding) : si les données font moins de 46 octets,
     * on ajoute des zéros pour atteindre le minimum de la trame */
    if (longueur < 46) {
        t.longueur_bourrage = 46 - longueur;
        memset(t.bourrage, 0, t.longueur_bourrage);
    } else {
        t.longueur_bourrage = 0;
        memset(t.bourrage, 0, 46);
    }

    /* FCS : normalement un CRC32, ici on met des zéros (simplifié) */
    memset(t.fcs, 0, 4);

    return t;
}

/* -----------------------------------------------------------------------
 * Fonctions de gestion de la table de commutation
 * ----------------------------------------------------------------------- */

/*
 * Cherche dans la table de commutation d'un switch si une MAC y est enregistrée.
 * Retourne le numéro de port, ou -1 si non trouvé.
 */
static int chercher_dans_table(Switch *sw, MAC mac) {
    for (int i = 0; i < sw->nb_entrees; i++) {
        if (mac_egales(sw->table[i].mac, mac))
            return sw->table[i].port;
    }
    return -1;  /* pas trouvé */
}

/*
 * Ajoute ou met à jour une entrée dans la table de commutation.
 * "J'ai reçu une trame depuis cette MAC sur ce port."
 */
static void apprendre_mac(Switch *sw, MAC mac, int port) {
    /* Si la MAC est déjà connue, mettre à jour le port */
    for (int i = 0; i < sw->nb_entrees; i++) {
        if (mac_egales(sw->table[i].mac, mac)) {
            sw->table[i].port = port;
            return;
        }
    }
    /* Sinon ajouter une nouvelle entrée (si la table n'est pas pleine) */
    if (sw->nb_entrees < 1024) {
        sw->table[sw->nb_entrees].mac  = mac;
        sw->table[sw->nb_entrees].port = port;
        sw->nb_entrees++;
    }
}

/* -----------------------------------------------------------------------
 * Navigation dans le graphe du réseau
 * ----------------------------------------------------------------------- */

int trouver_port_switch(LAN *lan, int id_switch, int id_voisin) {
    /*
     * Le numéro de port correspond à l'ordre dans lequel les liens
     * de ce switch apparaissent dans la liste des liens.
     * Ex : si le switch 0 est impliqué dans les liens 0, 1, 2,
     *      ces liens correspondent à ses ports 0, 1, 2.
     */
    int port = 0;
    for (int i = 0; i < lan->nb_liens; i++) {
        Lien *l = &lan->liens[i];
        int eq1 = l->eq1, eq2 = l->eq2;
        if (eq1 == id_switch || eq2 == id_switch) {
            /* Ce lien concerne notre switch */
            int voisin = (eq1 == id_switch) ? eq2 : eq1;
            if (voisin == id_voisin)
                return port;  /* trouvé ! */
            port++;
        }
    }
    return -1;  /* pas de connexion directe */
}

/*
 * Retourne la liste des voisins d'un switch avec leurs ports.
 * Remplit le tableau voisins[] et retourne le nombre de voisins.
 */
static int trouver_voisins_switch(LAN *lan, int id_switch,
                                  int voisins[], int ports[]) {
    int nb = 0;
    int port = 0;
    for (int i = 0; i < lan->nb_liens; i++) {
        Lien *l = &lan->liens[i];
        int eq1 = l->eq1, eq2 = l->eq2;
        if (eq1 == id_switch || eq2 == id_switch) {
            voisins[nb] = (eq1 == id_switch) ? eq2 : eq1;
            ports[nb]   = port;
            nb++;
            port++;
        }
    }
    return nb;
}

/*
 * Trouve le switch directement connecté à un équipement donné.
 * Retourne l'indice du switch, ou -1 si non trouvé.
 */
static int trouver_switch_voisin(LAN *lan, int id_equipement, int *port_switch) {
    for (int i = 0; i < lan->nb_liens; i++) {
        Lien *l = &lan->liens[i];
        int autre = -1;
        if (l->eq1 == id_equipement) autre = l->eq2;
        if (l->eq2 == id_equipement) autre = l->eq1;

        if (autre >= 0 && lan->equipements[autre].type == TYPE_SWITCH) {
            /* Trouver le numéro de port du switch pour cette connexion */
            *port_switch = trouver_port_switch(lan, autre, id_equipement);
            return autre;
        }
    }
    return -1;
}

/* -----------------------------------------------------------------------
 * Algorithme de diffusion d'une trame
 * ----------------------------------------------------------------------- */

/*
 * Traitement récursif : un switch reçoit une trame et décide quoi en faire.
 *
 * Paramètres :
 *   id_switch  : indice du switch qui traite la trame
 *   port_entree: numéro du port par lequel la trame est arrivée
 *   trame      : la trame reçue
 *   deja_vu[]  : tableau pour éviter les boucles infinies
 */
static void switch_traite_trame(LAN *lan, int id_switch, int port_entree,
                                 TrameEthernet *trame, int deja_vu[]) {
    /* Marquer ce switch immédiatement pour éviter qu'un autre switch
     * ne lui renvoie la trame pendant la récursion */
    deja_vu[id_switch] = 1;

    Switch *sw = &lan->equipements[id_switch].sw;

    printf("  [Switch %d] Reçoit la trame sur le port %d\n",
           id_switch, port_entree);

    /* Étape 1 : Apprendre l'adresse MAC source */
    apprendre_mac(sw, trame->source, port_entree);
    printf("  [Switch %d] Mémorise : MAC ", id_switch);
    afficher_mac(trame->source);
    printf(" est sur le port %d\n", port_entree);

    /* Étape 2 : Chercher la destination dans la table */
    int port_dest = -1;
    if (!est_broadcast(trame->destination)) {
        port_dest = chercher_dans_table(sw, trame->destination);
    }

    if (port_dest >= 0) {
        /* Destination connue : envoyer uniquement sur ce port */
        printf("  [Switch %d] Destination connue → port %d uniquement\n",
               id_switch, port_dest);
    } else {
        /* Destination inconnue ou broadcast : inonder tous les ports actifs */
        printf("  [Switch %d] Destination inconnue → diffusion sur tous les ports actifs\n",
               id_switch);
    }

    /* Étape 3 : Envoyer la trame aux voisins appropriés */
    int voisins[64], ports[64];
    int nb_voisins = trouver_voisins_switch(lan, id_switch, voisins, ports);

    for (int v = 0; v < nb_voisins; v++) {
        int port_sortie = ports[v];
        int id_voisin   = voisins[v];

        /* Ne pas renvoyer sur le port d'entrée */
        if (port_sortie == port_entree) continue;

        /* Vérifier que ce port est actif (pas bloqué par STP) */
        if (sw->etats_ports != NULL && sw->etats_ports[port_sortie] == PORT_BLOQUE) {
            printf("  [Switch %d] Port %d BLOQUÉ (STP) → trame non transmise\n",
                   id_switch, port_sortie);
            continue;
        }

        /* Si on cherche un port spécifique et que ce n'est pas le bon */
        if (port_dest >= 0 && port_sortie != port_dest) continue;

        /* Éviter les boucles (ne pas repasser deux fois par le même équipement) */
        if (deja_vu[id_voisin]) continue;

        printf("  [Switch %d] Transmet sur le port %d → équipement %d\n",
               id_switch, port_sortie, id_voisin);

        /* Si le voisin est un switch, il traite la trame à son tour */
        if (lan->equipements[id_voisin].type == TYPE_SWITCH) {
            int port_reception = trouver_port_switch(lan, id_voisin, id_switch);
            switch_traite_trame(lan, id_voisin, port_reception, trame, deja_vu);

        } else {
            /* Le voisin est une station */
            Station *dest = &lan->equipements[id_voisin].station;
            if (est_broadcast(trame->destination) ||
                mac_egales(dest->mac, trame->destination)) {
                printf("  [Station %d] Reçoit la trame ! (MAC: ", id_voisin);
                afficher_mac(dest->mac);
                printf(")\n");
            } else {
                printf("  [Station %d] Ignore la trame (pas pour elle)\n",
                       id_voisin);
            }
        }
    }
}

void diffuser_trame(LAN *lan, TrameEthernet *trame, int id_source) {
    printf("\n=== Diffusion d'une trame Ethernet ===\n");
    printf("Source      : ");
    afficher_mac(trame->source);
    printf(" (équipement %d)\n", id_source);
    printf("Destination : ");
    afficher_mac(trame->destination);
    if (est_broadcast(trame->destination)) printf(" (BROADCAST)");
    printf("\n\n");

    /* Tableau pour éviter les boucles infinies (un booléen par équipement) */
    int *deja_vu = calloc(lan->nb_equipements, sizeof(int));
    deja_vu[id_source] = 1;  /* on marque la source comme déjà traitée */

    /* Trouver le switch auquel est connectée la station source */
    int port_sw = -1;
    int id_switch = trouver_switch_voisin(lan, id_source, &port_sw);

    if (id_switch < 0) {
        printf("Erreur : la source n'est connectée à aucun switch\n");
        free(deja_vu);
        return;
    }

    printf("La trame arrive d'abord au switch %d sur le port %d\n\n",
           id_switch, port_sw);

    /* Le switch traite la trame */
    switch_traite_trame(lan, id_switch, port_sw, trame, deja_vu);

    free(deja_vu);
    printf("\n=== Fin de la diffusion ===\n\n");
}
