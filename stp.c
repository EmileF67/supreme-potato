/*
 * stp.c - Protocole STP (Spanning Tree Protocol / 802.1d)
 *
 * Le STP sert à éviter les boucles dans un réseau avec des chemins redondants.
 * Il construit un "arbre" à partir du graphe du réseau en bloquant certains ports.
 *
 * Rappel du principe :
 *   1. Élection de la racine : le switch avec le plus petit ID (priorité + MAC)
 *   2. Chaque switch trouve son "port racine" (le plus court chemin vers la racine)
 *   3. Sur chaque segment, un seul switch a un "port désigné" (actif)
 *   4. Les autres ports sont bloqués
 *
 * Mécanisme : les switchs s'échangent des BPDU (Bridge Protocol Data Unit).
 * Un BPDU contient : [ID_racine, coût_vers_racine, ID_émetteur]
 *
 * Comparaison de BPDUs (meilleur = plus petit) :
 *   B1 < B2 si :
 *     - ID_racine(B1) < ID_racine(B2)
 *     - ou ID_racines égaux et coût(B1) < coût(B2)
 *     - ou ID_racines et coûts égaux et ID_émetteur(B1) < ID_émetteur(B2)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stp.h"
#include "affichage.h"

/* -----------------------------------------------------------------------
 * Comparaison d'identifiants de switch
 * L'ID d'un switch = priorité (16 bits) + MAC (48 bits)
 * Plus petit = plus prioritaire (devient la racine)
 * ----------------------------------------------------------------------- */

/*
 * Compare deux switchs : retourne -1 si sw1 < sw2, 0 si égaux, +1 si sw1 > sw2.
 * On compare d'abord la priorité, puis la MAC.
 */
static int comparer_switch_id(int prio1, MAC mac1, int prio2, MAC mac2) {
    if (prio1 != prio2)
        return (prio1 < prio2) ? -1 : 1;
    /* Même priorité : comparer les MACs octet par octet */
    return memcmp(mac1.octets, mac2.octets, 6);
}

/*
 * Compare deux BPDUs.
 * Retourne -1 si b1 est meilleur que b2, 0 si égaux, +1 si b2 est meilleur.
 */
static int comparer_bpdu(BPDU *b1, BPDU *b2) {
    /* 1. Comparer l'ID de la racine */
    int cmp = comparer_switch_id(b1->priorite_racine, b1->mac_racine,
                                  b2->priorite_racine, b2->mac_racine);
    if (cmp != 0) return cmp;

    /* 2. Comparer le coût vers la racine */
    if (b1->cout != b2->cout)
        return (b1->cout < b2->cout) ? -1 : 1;

    /* 3. Comparer l'ID de l'émetteur */
    return comparer_switch_id(b1->priorite_emetteur, b1->mac_emetteur,
                               b2->priorite_emetteur, b2->mac_emetteur);
}

/* -----------------------------------------------------------------------
 * Structures internes de l'algorithme STP
 * ----------------------------------------------------------------------- */

/*
 * État STP d'un switch pendant le calcul.
 * Chaque switch maintient :
 *   - le meilleur BPDU qu'il a reçu sur chaque port
 *   - le BPDU qu'il va envoyer (sa "proposition")
 */
typedef struct {
    int    id;           /* indice du switch dans le LAN */
    BPDU   bpdu_envoye;  /* BPDU que ce switch diffuse */
    BPDU  *bpdu_recus;   /* meilleur BPDU reçu sur chaque port (tableau) */
    int    nb_ports;     /* nombre de ports */
    int    port_racine;  /* numéro du port racine (-1 si ce switch est la racine) */
} EtatSTP;

/* -----------------------------------------------------------------------
 * Algorithme principal
 * ----------------------------------------------------------------------- */

void executer_stp(LAN *lan) {
    /* Compter le nombre de switchs */
    int nb_switchs = 0;
    for (int i = 0; i < lan->nb_equipements; i++)
        if (lan->equipements[i].type == TYPE_SWITCH) nb_switchs++;

    if (nb_switchs == 0) {
        printf("STP : aucun switch dans le réseau.\n");
        return;
    }

    printf("=== Protocole STP - Démarrage (%d switchs) ===\n\n", nb_switchs);

    /* Construire un tableau qui fait le lien entre l'indice dans le LAN
     * et l'indice dans notre tableau d'états STP */
    int *idx_stp = malloc(lan->nb_equipements * sizeof(int));
    for (int i = 0; i < lan->nb_equipements; i++) idx_stp[i] = -1;

    /* Allouer et initialiser l'état STP de chaque switch */
    EtatSTP *etats = malloc(nb_switchs * sizeof(EtatSTP));
    int s = 0;
    for (int i = 0; i < lan->nb_equipements; i++) {
        if (lan->equipements[i].type != TYPE_SWITCH) continue;
        Switch *sw = &lan->equipements[i].sw;

        idx_stp[i] = s;
        etats[s].id         = i;
        etats[s].nb_ports   = sw->nb_ports;
        etats[s].port_racine = -1;

        /* Au départ, chaque switch pense être la racine */
        /* Il envoie un BPDU : "Racine = moi, coût = 0, Émetteur = moi" */
        etats[s].bpdu_envoye.priorite_racine   = sw->priorite;
        etats[s].bpdu_envoye.mac_racine        = sw->mac;
        etats[s].bpdu_envoye.cout              = 0;
        etats[s].bpdu_envoye.priorite_emetteur = sw->priorite;
        etats[s].bpdu_envoye.mac_emetteur      = sw->mac;

        /* Initialiser les BPDUs reçus avec "soi-même" (pire cas = pas de BPDU) */
        etats[s].bpdu_recus = malloc(sw->nb_ports * sizeof(BPDU));
        for (int p = 0; p < sw->nb_ports; p++) {
            etats[s].bpdu_recus[p] = etats[s].bpdu_envoye;
        }
        s++;
    }

    /*
     * Boucle principale : on simule les échanges de BPDUs jusqu'à convergence.
     * À chaque round, chaque switch envoie son BPDU à ses voisins switch.
     * On s'arrête quand rien ne change pendant un round complet.
     */
    int changement = 1;
    int round = 0;

    while (changement) {
        changement = 0;
        round++;
        printf("--- Round %d ---\n", round);

        /* Pour chaque switch, envoyer son BPDU à tous ses voisins switch */
        for (int s1 = 0; s1 < nb_switchs; s1++) {
            int id1 = etats[s1].id;

            /* Parcourir tous les liens pour trouver les voisins switch */
            for (int l = 0; l < lan->nb_liens; l++) {
                Lien *lien = &lan->liens[l];
                int autre = -1;
                int port_s1 = -1;  /* port sur lequel s1 est connecté à autre */

                if (lien->eq1 == id1) { autre = lien->eq2; }
                if (lien->eq2 == id1) { autre = lien->eq1; }
                if (autre < 0) continue;

                /* Le voisin doit être un switch */
                if (lan->equipements[autre].type != TYPE_SWITCH) continue;

                int s2 = idx_stp[autre];
                if (s2 < 0) continue;

                /* Trouver le numéro de port chez s2 qui mène vers s1 */
                int port_reception = -1;
                {
                    int p = 0;
                    for (int ll = 0; ll < lan->nb_liens; ll++) {
                        Lien *ll2 = &lan->liens[ll];
                        int a1 = ll2->eq1, a2 = ll2->eq2;
                        if (a1 == autre || a2 == autre) {
                            int voisin = (a1 == autre) ? a2 : a1;
                            if (voisin == id1) { port_reception = p; break; }
                            p++;
                        }
                    }
                }
                /* Trouver le numéro de port chez s1 qui mène vers s2 */
                {
                    int p = 0;
                    for (int ll = 0; ll < lan->nb_liens; ll++) {
                        Lien *ll2 = &lan->liens[ll];
                        int a1 = ll2->eq1, a2 = ll2->eq2;
                        if (a1 == id1 || a2 == id1) {
                            int voisin = (a1 == id1) ? a2 : a1;
                            if (voisin == autre) { port_s1 = p; break; }
                            p++;
                        }
                    }
                }

                if (port_reception < 0 || port_s1 < 0) continue;
                if (port_reception >= etats[s2].nb_ports) continue;

                /*
                 * s1 envoie son BPDU à s2 sur le port port_reception.
                 * Le BPDU reçu par s2 a un coût augmenté du coût du lien.
                 */
                BPDU bpdu_arrive = etats[s1].bpdu_envoye;
                bpdu_arrive.cout += lien->poids;

                /* s2 garde le meilleur BPDU reçu sur ce port */
                if (comparer_bpdu(&bpdu_arrive, &etats[s2].bpdu_recus[port_reception]) < 0) {
                    etats[s2].bpdu_recus[port_reception] = bpdu_arrive;
                    changement = 1;
                }
            }
        }

        /*
         * Après réception des BPDUs, chaque switch recalcule le meilleur BPDU
         * qu'il connaît (sur tous ses ports) pour mettre à jour son propre BPDU.
         */
        for (int s1 = 0; s1 < nb_switchs; s1++) {
            Switch *sw = &lan->equipements[etats[s1].id].sw;

            /* Trouver le meilleur BPDU parmi tous les ports */
            BPDU meilleur = etats[s1].bpdu_envoye;  /* commence avec soi-même */
            int  best_port = -1;

            for (int p = 0; p < etats[s1].nb_ports; p++) {
                if (comparer_bpdu(&etats[s1].bpdu_recus[p], &meilleur) < 0) {
                    meilleur   = etats[s1].bpdu_recus[p];
                    best_port  = p;
                }
            }

            etats[s1].port_racine = best_port;

            /*
             * Ce switch met à jour le BPDU qu'il va diffuser :
             * "Racine = racine du meilleur BPDU, coût = coût du meilleur + 0,
             *  Émetteur = moi"
             */
            BPDU nouveau_bpdu = meilleur;
            nouveau_bpdu.priorite_emetteur = sw->priorite;
            nouveau_bpdu.mac_emetteur      = sw->mac;
            /* Le coût est déjà inclus dans le meilleur BPDU reçu */

            if (comparer_bpdu(&nouveau_bpdu, &etats[s1].bpdu_envoye) != 0) {
                etats[s1].bpdu_envoye = nouveau_bpdu;
                changement = 1;
            }
        }
    }

    printf("\nConvergence atteinte en %d rounds.\n\n", round);

    /* -----------------------------------------------------------------------
     * Détermination des états finaux des ports
     * ----------------------------------------------------------------------- */

    /*
     * Règles :
     *   - Le switch avec le plus petit ID est la racine.
     *     Tous ses ports sont DÉSIGNÉS.
     *   - Pour chaque autre switch : le port par lequel il reçoit le meilleur
     *     BPDU est son PORT RACINE.
     *   - Pour chaque lien entre deux switchs :
     *       → Le switch qui envoie le meilleur BPDU a un port DÉSIGNÉ
     *       → L'autre a un port BLOQUÉ
     */

    /* Trouver la racine : le switch avec le plus petit ID */
    int idx_racine = 0;
    for (int s1 = 1; s1 < nb_switchs; s1++) {
        Switch *sw1 = &lan->equipements[etats[s1].id].sw;
        Switch *sw0 = &lan->equipements[etats[idx_racine].id].sw;
        if (comparer_switch_id(sw1->priorite, sw1->mac,
                                sw0->priorite, sw0->mac) < 0) {
            idx_racine = s1;
        }
    }

    Switch *sw_racine = &lan->equipements[etats[idx_racine].id].sw;
    printf("Switch racine : équipement %d (priorité %d, MAC: ",
           etats[idx_racine].id, sw_racine->priorite);
    afficher_mac(sw_racine->mac);
    printf(")\n\n");

    /* Remettre tous les ports à INCONNU pour les reconfigurer */
    for (int s1 = 0; s1 < nb_switchs; s1++) {
        Switch *sw = &lan->equipements[etats[s1].id].sw;
        for (int p = 0; p < sw->nb_ports; p++)
            sw->etats_ports[p] = PORT_INCONNU;
    }

    /* La racine : tous ses ports sont DÉSIGNÉS */
    {
        Switch *sw = &lan->equipements[etats[idx_racine].id].sw;
        for (int p = 0; p < sw->nb_ports; p++)
            sw->etats_ports[p] = PORT_DESIGNE;
    }

    /* Pour chaque autre switch : configurer le PORT RACINE */
    for (int s1 = 0; s1 < nb_switchs; s1++) {
        if (s1 == idx_racine) continue;
        Switch *sw = &lan->equipements[etats[s1].id].sw;
        int pr = etats[s1].port_racine;
        if (pr >= 0 && pr < sw->nb_ports)
            sw->etats_ports[pr] = PORT_RACINE;
    }

    /* Pour chaque lien switch-switch : désigné vs bloqué */
    for (int l = 0; l < lan->nb_liens; l++) {
        Lien *lien = &lan->liens[l];
        int id1 = lien->eq1, id2 = lien->eq2;
        if (lan->equipements[id1].type != TYPE_SWITCH) continue;
        if (lan->equipements[id2].type != TYPE_SWITCH) continue;

        int s1 = idx_stp[id1];
        int s2 = idx_stp[id2];
        if (s1 < 0 || s2 < 0) continue;

        /* Trouver les numéros de ports de chaque côté */
        int port1 = -1, port2 = -1;
        {
            int p = 0;
            for (int ll = 0; ll < lan->nb_liens; ll++) {
                Lien *ll2 = &lan->liens[ll];
                if (ll2->eq1 == id1 || ll2->eq2 == id1) {
                    int v = (ll2->eq1==id1)?ll2->eq2:ll2->eq1;
                    if (v == id2) { port1 = p; break; }
                    p++;
                }
            }
        }
        {
            int p = 0;
            for (int ll = 0; ll < lan->nb_liens; ll++) {
                Lien *ll2 = &lan->liens[ll];
                if (ll2->eq1 == id2 || ll2->eq2 == id2) {
                    int v = (ll2->eq1==id2)?ll2->eq2:ll2->eq1;
                    if (v == id1) { port2 = p; break; }
                    p++;
                }
            }
        }

        if (port1 < 0 || port2 < 0) continue;

        Switch *sw1 = &lan->equipements[id1].sw;
        Switch *sw2 = &lan->equipements[id2].sw;

        /* Ne pas écraser un PORT_RACINE déjà configuré */
        if (sw1->etats_ports[port1] == PORT_RACINE) continue;
        if (sw2->etats_ports[port2] == PORT_RACINE) continue;

        /*
         * Le switch qui a le meilleur BPDU à envoyer obtient le port DÉSIGNÉ.
         * L'autre est BLOQUÉ.
         */
        if (comparer_bpdu(&etats[s1].bpdu_envoye, &etats[s2].bpdu_envoye) <= 0) {
            sw1->etats_ports[port1] = PORT_DESIGNE;
            sw2->etats_ports[port2] = PORT_BLOQUE;
        } else {
            sw1->etats_ports[port1] = PORT_BLOQUE;
            sw2->etats_ports[port2] = PORT_DESIGNE;
        }
    }

    /* Les ports vers les stations sont toujours DÉSIGNÉS */
    for (int l = 0; l < lan->nb_liens; l++) {
        Lien *lien = &lan->liens[l];
        int id1 = lien->eq1, id2 = lien->eq2;

        if (lan->equipements[id1].type == TYPE_SWITCH &&
            lan->equipements[id2].type == TYPE_STATION) {
            int s1 = idx_stp[id1];
            int port = -1;
            int p = 0;
            for (int ll = 0; ll < lan->nb_liens; ll++) {
                Lien *ll2 = &lan->liens[ll];
                if (ll2->eq1 == id1 || ll2->eq2 == id1) {
                    int v = (ll2->eq1==id1)?ll2->eq2:ll2->eq1;
                    if (v == id2) { port = p; break; }
                    p++;
                }
            }
            if (port >= 0 && s1 >= 0)
                lan->equipements[id1].sw.etats_ports[port] = PORT_DESIGNE;
        }
        /* Idem dans l'autre sens */
        if (lan->equipements[id2].type == TYPE_SWITCH &&
            lan->equipements[id1].type == TYPE_STATION) {
            int s2 = idx_stp[id2];
            int port = -1;
            int p = 0;
            for (int ll = 0; ll < lan->nb_liens; ll++) {
                Lien *ll2 = &lan->liens[ll];
                if (ll2->eq1 == id2 || ll2->eq2 == id2) {
                    int v = (ll2->eq1==id2)?ll2->eq2:ll2->eq1;
                    if (v == id1) { port = p; break; }
                    p++;
                }
            }
            if (port >= 0 && s2 >= 0)
                lan->equipements[id2].sw.etats_ports[port] = PORT_DESIGNE;
        }
    }

    /* Libérer la mémoire temporaire */
    for (int s1 = 0; s1 < nb_switchs; s1++)
        free(etats[s1].bpdu_recus);
    free(etats);
    free(idx_stp);

    printf("Configuration STP terminée.\n\n");
}
