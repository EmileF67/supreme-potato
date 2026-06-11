/*
 * fichier.c - Chargement d'un réseau depuis un fichier de configuration
 *
 * Ce fichier gère la lecture du format de fichier imposé par le projet :
 *
 *   4 3
 *   2;01:45:23:a6:f7:ab;8;1024      <- switch (type 2)
 *   1;54:d6:a6:82:c5:23;130.79.80.21 <- station (type 1)
 *   1;c8:69:72:5e:43:af;130.79.80.27
 *   1;77:ac:d6:82:12:23;130.79.80.42
 *   0;1;4                             <- lien entre équip. 0 et 1, coût 4
 *   0;2;19
 *   0;3;4
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fichier.h"

/* -----------------------------------------------------------------------
 * Fonctions internes (utilitaires de parsing)
 * ----------------------------------------------------------------------- */

/*
 * Lit une adresse MAC depuis une chaîne "xx:xx:xx:xx:xx:xx"
 * et la stocke dans la structure MAC.
 */
static MAC lire_mac(const char *texte) {
    MAC mac;
    /* sscanf lit 6 entiers hexadécimaux séparés par ':' */
    unsigned int b[6];
    sscanf(texte, "%x:%x:%x:%x:%x:%x",
           &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]);
    for (int i = 0; i < 6; i++)
        mac.octets[i] = (uint8_t)b[i];
    return mac;
}

/*
 * Lit une adresse IPv4 depuis une chaîne "a.b.c.d"
 * et la stocke dans la structure IPv4.
 */
static IPv4 lire_ip(const char *texte) {
    IPv4 ip;
    unsigned int b[4];
    sscanf(texte, "%u.%u.%u.%u", &b[0], &b[1], &b[2], &b[3]);
    for (int i = 0; i < 4; i++)
        ip.octets[i] = (uint8_t)b[i];
    return ip;
}

/*
 * Analyse une ligne d'équipement.
 * Format switch  : "2;MAC;ports;priorité"
 * Format station : "1;MAC;IP"
 */
static void lire_equipement(Equipement *eq, const char *ligne) {
    /* Copie de la ligne pour pouvoir la découper avec strtok */
    char buf[256];
    strncpy(buf, ligne, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    /* Supprime le retour à la ligne éventuel */
    char *nl = strchr(buf, '\n');
    if (nl) *nl = '\0';
    nl = strchr(buf, '\r');
    if (nl) *nl = '\0';

    /* Premier champ : type (1=station, 2=switch) */
    char *token = strtok(buf, ";");
    eq->type = (TypeEquipement)atoi(token);

    if (eq->type == TYPE_SWITCH) {
        /* Switch : MAC ; nb_ports ; priorité */
        token = strtok(NULL, ";");
        eq->sw.mac = lire_mac(token);

        token = strtok(NULL, ";");
        eq->sw.nb_ports = atoi(token);

        token = strtok(NULL, ";");
        eq->sw.priorite = atoi(token);

        /* Table de commutation vide au départ */
        eq->sw.nb_entrees = 0;

        /* Allocation du tableau d'états de ports */
        eq->sw.etats_ports = calloc(eq->sw.nb_ports, sizeof(EtatPort));
        /* Tous les ports sont INCONNU au départ */
        for (int i = 0; i < eq->sw.nb_ports; i++)
            eq->sw.etats_ports[i] = PORT_INCONNU;

    } else {
        /* Station : MAC ; IP */
        token = strtok(NULL, ";");
        eq->station.mac = lire_mac(token);

        token = strtok(NULL, ";");
        eq->station.ip = lire_ip(token);
    }
}

/*
 * Analyse une ligne de lien.
 * Format : "eq1;eq2;coût"
 */
static Lien lire_lien(const char *ligne) {
    Lien l;
    char buf[128];
    strncpy(buf, ligne, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *nl = strchr(buf, '\n');
    if (nl) *nl = '\0';
    nl = strchr(buf, '\r');
    if (nl) *nl = '\0';

    char *token = strtok(buf, ";");
    l.eq1 = atoi(token);

    token = strtok(NULL, ";");
    l.eq2 = atoi(token);

    token = strtok(NULL, ";");
    l.poids = atoi(token);

    return l;
}

/* -----------------------------------------------------------------------
 * Fonctions publiques
 * ----------------------------------------------------------------------- */

LAN *charger_lan(const char *chemin_fichier) {
    FILE *f = fopen(chemin_fichier, "r");
    if (f == NULL) {
        fprintf(stderr, "Erreur : impossible d'ouvrir '%s'\n", chemin_fichier);
        return NULL;
    }

    /* Allouer la structure LAN */
    LAN *lan = malloc(sizeof(LAN));
    if (lan == NULL) {
        fclose(f);
        return NULL;
    }

    /* Lire la première ligne : nb_équipements nb_liens */
    fscanf(f, "%d %d\n", &lan->nb_equipements, &lan->nb_liens);

    /* Allouer les tableaux */
    lan->equipements = malloc(lan->nb_equipements * sizeof(Equipement));
    lan->liens       = malloc(lan->nb_liens       * sizeof(Lien));

    if (lan->equipements == NULL || lan->liens == NULL) {
        fprintf(stderr, "Erreur : mémoire insuffisante\n");
        fclose(f);
        free(lan->equipements);
        free(lan->liens);
        free(lan);
        return NULL;
    }

    /* Lire chaque équipement */
    char ligne[256];
    for (int i = 0; i < lan->nb_equipements; i++) {
        if (fgets(ligne, sizeof(ligne), f) == NULL) {
            fprintf(stderr, "Erreur de lecture à l'équipement %d\n", i);
            break;
        }
        lire_equipement(&lan->equipements[i], ligne);
    }

    /* Lire chaque lien */
    for (int i = 0; i < lan->nb_liens; i++) {
        if (fgets(ligne, sizeof(ligne), f) == NULL) {
            fprintf(stderr, "Erreur de lecture au lien %d\n", i);
            break;
        }
        lan->liens[i] = lire_lien(ligne);
    }

    fclose(f);

    /*
     * Correction : s'assurer que chaque switch a assez d'entrées dans
     * etats_ports pour couvrir toutes ses connexions réelles.
     * (Un fichier peut déclarer nb_ports < nombre de liens réels.)
     */
    for (int i = 0; i < lan->nb_equipements; i++) {
        if (lan->equipements[i].type != TYPE_SWITCH) continue;
        Switch *sw = &lan->equipements[i].sw;

        /* Compter les connexions réelles de ce switch */
        int connexions = 0;
        for (int l = 0; l < lan->nb_liens; l++) {
            if (lan->liens[l].eq1 == i || lan->liens[l].eq2 == i)
                connexions++;
        }

        if (connexions > sw->nb_ports) {
            /* Réallouer avec la bonne taille */
            free(sw->etats_ports);
            sw->etats_ports = calloc(connexions, sizeof(EtatPort));
            sw->nb_ports = connexions;
        }
    }

    return lan;
}

int sauvegarder_lan(LAN *lan, const char *chemin_fichier) {
    FILE *f = fopen(chemin_fichier, "w");
    if (f == NULL) {
        fprintf(stderr, "Erreur : impossible de créer '%s'\n", chemin_fichier);
        return -1;
    }

    /* Ligne d'en-tête : nb_équipements nb_liens */
    fprintf(f, "%d %d\n", lan->nb_equipements, lan->nb_liens);

    /* Écrire chaque équipement */
    for (int i = 0; i < lan->nb_equipements; i++) {
        Equipement *eq = &lan->equipements[i];
        if (eq->type == TYPE_SWITCH) {
            Switch *sw = &eq->sw;
            fprintf(f, "2;%02x:%02x:%02x:%02x:%02x:%02x;%d;%d\n",
                    sw->mac.octets[0], sw->mac.octets[1], sw->mac.octets[2],
                    sw->mac.octets[3], sw->mac.octets[4], sw->mac.octets[5],
                    sw->nb_ports, sw->priorite);
        } else {
            Station *st = &eq->station;
            fprintf(f, "1;%02x:%02x:%02x:%02x:%02x:%02x;%d.%d.%d.%d\n",
                    st->mac.octets[0], st->mac.octets[1], st->mac.octets[2],
                    st->mac.octets[3], st->mac.octets[4], st->mac.octets[5],
                    st->ip.octets[0], st->ip.octets[1],
                    st->ip.octets[2], st->ip.octets[3]);
        }
    }

    /* Écrire chaque lien */
    for (int i = 0; i < lan->nb_liens; i++) {
        fprintf(f, "%d;%d;%d\n",
                lan->liens[i].eq1, lan->liens[i].eq2, lan->liens[i].poids);
    }

    fclose(f);
    return 0;
}

void liberer_lan(LAN *lan) {
    if (lan == NULL) return;

    /* Libérer le tableau d'états de ports de chaque switch */
    for (int i = 0; i < lan->nb_equipements; i++) {
        if (lan->equipements[i].type == TYPE_SWITCH) {
            free(lan->equipements[i].sw.etats_ports);
        }
    }

    free(lan->equipements);
    free(lan->liens);
    free(lan);
}
