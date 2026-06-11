/*
 * affichage.c - Fonctions d'affichage
 *
 * Toutes les fonctions qui permettent d'afficher les structures de données
 * de façon lisible pour un humain.
 */

#include <stdio.h>
#include <string.h>
#include "affichage.h"

/* Noms des états de port STP */
static const char *nom_etat[] = {
    "INCONNU", "RACINE", "DÉSIGNÉ", "BLOQUÉ"
};

/* -----------------------------------------------------------------------
 * Affichage des adresses
 * ----------------------------------------------------------------------- */

void afficher_mac(MAC mac) {
    /* Affiche 6 octets en hexadécimal séparés par ":" */
    printf("%02x:%02x:%02x:%02x:%02x:%02x",
           mac.octets[0], mac.octets[1], mac.octets[2],
           mac.octets[3], mac.octets[4], mac.octets[5]);
}

void afficher_ip(IPv4 ip) {
    /* Affiche 4 octets en décimal séparés par "." */
    printf("%d.%d.%d.%d",
           ip.octets[0], ip.octets[1], ip.octets[2], ip.octets[3]);
}

/* -----------------------------------------------------------------------
 * Affichage des équipements
 * ----------------------------------------------------------------------- */

void afficher_station(Station *s, int indice) {
    printf("  Station %d :\n", indice);
    printf("    MAC : "); afficher_mac(s->mac); printf("\n");
    printf("    IP  : "); afficher_ip(s->ip);   printf("\n");
}

void afficher_switch(Switch *sw, int indice) {
    printf("  Switch %d :\n", indice);
    printf("    MAC      : "); afficher_mac(sw->mac); printf("\n");
    printf("    Ports    : %d\n", sw->nb_ports);
    printf("    Priorité : %d\n", sw->priorite);

    /* Affichage de la table de commutation */
    if (sw->nb_entrees == 0) {
        printf("    Table de commutation : (vide)\n");
    } else {
        printf("    Table de commutation :\n");
        for (int i = 0; i < sw->nb_entrees; i++) {
            printf("      "); afficher_mac(sw->table[i].mac);
            printf(" -> port %d\n", sw->table[i].port);
        }
    }

    /* Affichage de l'état des ports (si configuré par STP) */
    if (sw->etats_ports != NULL) {
        printf("    États des ports :\n");
        for (int p = 0; p < sw->nb_ports; p++) {
            printf("      Port %d : %s\n", p, nom_etat[sw->etats_ports[p]]);
        }
    }
}

/* -----------------------------------------------------------------------
 * Affichage du réseau complet
 * ----------------------------------------------------------------------- */

void afficher_lan(LAN *lan) {
    printf("=== Réseau local (%d équipements, %d liens) ===\n\n",
           lan->nb_equipements, lan->nb_liens);

    printf("--- Équipements ---\n");
    for (int i = 0; i < lan->nb_equipements; i++) {
        Equipement *eq = &lan->equipements[i];
        if (eq->type == TYPE_SWITCH) {
            afficher_switch(&eq->sw, i);
        } else {
            afficher_station(&eq->station, i);
        }
        printf("\n");
    }

    printf("--- Liens ---\n");
    for (int i = 0; i < lan->nb_liens; i++) {
        Lien *l = &lan->liens[i];
        printf("  Équipement %d <--> Équipement %d  (coût : %d)\n",
               l->eq1, l->eq2, l->poids);
    }
    printf("\n");
}

/* -----------------------------------------------------------------------
 * Affichage des trames Ethernet
 * ----------------------------------------------------------------------- */

void afficher_trame(TrameEthernet *t) {
    printf("=== Trame Ethernet ===\n");
    printf("  Source      : "); afficher_mac(t->source);      printf("\n");
    printf("  Destination : "); afficher_mac(t->destination); printf("\n");

    /* Décodage du champ type */
    printf("  Type        : 0x%04X", t->type);
    if      (t->type == 0x0800) printf(" (IPv4)");
    else if (t->type == 0x0806) printf(" (ARP)");
    else if (t->type == 0x86DD) printf(" (IPv6)");
    printf("\n");

    printf("  Données     : %d octet(s)\n", t->longueur_donnees);
    printf("\n");
}

void afficher_trame_hex(TrameEthernet *t) {
    printf("=== Trame Ethernet (hexadécimal brut) ===\n");

    /* Préambule */
    printf("  Préambule : ");
    for (int i = 0; i < 7; i++) printf("%02X ", t->preambule[i]);
    printf("\n");

    /* SFD */
    printf("  SFD       : %02X\n", t->sfd);

    /* Destination */
    printf("  Dest MAC  : ");
    for (int i = 0; i < 6; i++) printf("%02X ", t->destination.octets[i]);
    printf("\n");

    /* Source */
    printf("  Src MAC   : ");
    for (int i = 0; i < 6; i++) printf("%02X ", t->source.octets[i]);
    printf("\n");

    /* Type */
    printf("  Type      : %02X %02X\n", (t->type >> 8) & 0xFF, t->type & 0xFF);

    /* Données (max 16 premiers octets) */
    int nb = t->longueur_donnees;
    if (nb > 16) nb = 16;
    printf("  Données   : ");
    for (int i = 0; i < nb; i++) printf("%02X ", t->donnees[i]);
    if (t->longueur_donnees > 16) printf("...");
    printf("(%d octet(s))\n", t->longueur_donnees);

    /* Bourrage */
    if (t->longueur_bourrage > 0) {
        printf("  Bourrage  : ");
        for (int i = 0; i < t->longueur_bourrage; i++) printf("%02X ", t->bourrage[i]);
        printf("(%d octet(s))\n", t->longueur_bourrage);
    }

    /* FCS */
    printf("  FCS       : ");
    for (int i = 0; i < 4; i++) printf("%02X ", t->fcs[i]);
    printf("\n\n");
}

/* -----------------------------------------------------------------------
 * Affichage des résultats STP
 * ----------------------------------------------------------------------- */

void afficher_resultats_stp(LAN *lan) {
    printf("=== Résultats du protocole STP ===\n\n");

    for (int i = 0; i < lan->nb_equipements; i++) {
        if (lan->equipements[i].type != TYPE_SWITCH) continue;
        Switch *sw = &lan->equipements[i].sw;

        printf("Switch %d (MAC: ", i);
        afficher_mac(sw->mac);
        printf(", priorité: %d)\n", sw->priorite);

        /*
         * Les ports sont numérotés dans l'ordre des liens qui impliquent ce switch.
         * On les affiche avec l'équipement voisin correspondant.
         */
        int port = 0;
        for (int l = 0; l < lan->nb_liens; l++) {
            Lien *lien = &lan->liens[l];
            int voisin = -1;
            if (lien->eq1 == i) voisin = lien->eq2;
            if (lien->eq2 == i) voisin = lien->eq1;
            if (voisin < 0) continue;

            if (port < sw->nb_ports) {
                printf("  Port %d : %-10s  → équipement %d",
                       port, nom_etat[sw->etats_ports[port]], voisin);
                if (lan->equipements[voisin].type == TYPE_SWITCH)
                    printf(" (switch)");
                else
                    printf(" (station)");
                printf("\n");
            }
            port++;
        }
        printf("\n");
    }
}
