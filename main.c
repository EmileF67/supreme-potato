/*
 * main.c - Programme principal de démonstration
 *
 * Ce programme démontre les 4 étapes du projet SAE 2.3 :
 *   Étape 1 : Affichage des structures de données (MAC, IP, Station, Switch)
 *   Étape 2 : Chargement d'un réseau depuis un fichier de configuration
 *   Étape 3 : Simulation d'une diffusion de trame Ethernet
 *   Étape 4 : Protocole STP (calcul de l'arbre de recouvrement)
 *
 * Usage : ./sae [fichier_config]
 *         Si aucun fichier n'est donné, utilise configs/config2.txt
 *
 * ARCHITECTURE : Événementielle (Event-driven)
 * Les 4 étapes sont exécutées par une boucle d'événements,
 * chaque étape étant un événement traité séquentiellement.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "affichage.h"
#include "fichier.h"
#include "ethernet.h"
#include "stp.h"
#include "evenement.h"

/* -----------------------------------------------------------------------
 * Gestionnaires d'événements (handlers)
 *
 * Chaque gestionnaire traite un événement spécifique.
 * ----------------------------------------------------------------------- */

/* Gestionnaire : EVENT_DEMO_ETAPE1 */
static void traiter_etape1(Evenement evt) {
    (void)evt;  /* Non utilisé pour cette étape */
    
    printf("╔════════════════════════════════════════╗\n");
    printf("║  ÉTAPE 1 : Structures de données       ║\n");
    printf("╚════════════════════════════════════════╝\n\n");

    /* Créer une adresse MAC manuellement */
    MAC mac_exemple = {{0x01, 0x45, 0x23, 0xa6, 0xf7, 0xab}};
    printf("Exemple d'adresse MAC : ");
    afficher_mac(mac_exemple);
    printf("\n");

    /* Créer une adresse IP manuellement */
    IPv4 ip_exemple = {{130, 79, 80, 21}};
    printf("Exemple d'adresse IP  : ");
    afficher_ip(ip_exemple);
    printf("\n\n");

    /* Créer une station */
    Station st;
    st.mac = mac_exemple;
    st.ip  = ip_exemple;
    afficher_station(&st, 0);
    printf("\n");

    /* Créer un switch */
    Switch sw;
    sw.mac       = (MAC){{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};
    sw.nb_ports  = 4;
    sw.priorite  = 32768;
    sw.nb_entrees = 0;
    sw.etats_ports = calloc(sw.nb_ports, sizeof(EtatPort));

    /* Ajouter une entrée dans la table de commutation */
    sw.table[0].mac  = (MAC){{0x54, 0xd6, 0xa6, 0x82, 0xc5, 0x01}};
    sw.table[0].port = 2;
    sw.nb_entrees = 1;

    afficher_switch(&sw, 0);
    free(sw.etats_ports);
    printf("\n");
}

/* Gestionnaire : EVENT_DEMO_ETAPE2 */
static void traiter_etape2(Evenement evt) {
    const char *fichier = evt.donnees.fichier;
    
    printf("╔════════════════════════════════════════╗\n");
    printf("║  ÉTAPE 2 : Chargement depuis fichier   ║\n");
    printf("╚════════════════════════════════════════╝\n\n");

    printf("Chargement de : %s\n\n", fichier);
    LAN *lan = charger_lan(fichier);
    if (lan == NULL) {
        printf("Erreur : impossible de charger le fichier.\n\n");
        return;
    }

    afficher_lan(lan);

    /* Test de la sauvegarde : on réécrit le réseau dans un nouveau fichier */
    const char *fichier_sortie = "reseau_sauvegarde.txt";
    if (sauvegarder_lan(lan, fichier_sortie) == 0) {
        printf("Réseau sauvegardé dans : %s\n\n", fichier_sortie);
    }

    liberer_lan(lan);
}

/* Gestionnaire : EVENT_DEMO_ETAPE3 */
static void traiter_etape3(Evenement evt) {
    const char *fichier = evt.donnees.fichier;
    
    printf("╔════════════════════════════════════════╗\n");
    printf("║  ÉTAPE 3 : Commutation Ethernet        ║\n");
    printf("╚════════════════════════════════════════╝\n\n");

    LAN *lan = charger_lan(fichier);
    if (lan == NULL) return;

    /* Trouver la première et la deuxième station du réseau */
    int id_src = -1, id_dst = -1;
    for (int i = 0; i < lan->nb_equipements; i++) {
        if (lan->equipements[i].type == TYPE_STATION) {
            if      (id_src < 0) id_src = i;
            else if (id_dst < 0) { id_dst = i; break; }
        }
    }

    if (id_src < 0 || id_dst < 0) {
        printf("Pas assez de stations pour la démonstration.\n\n");
        liberer_lan(lan);
        return;
    }

    Station *src = &lan->equipements[id_src].station;
    Station *dst = &lan->equipements[id_dst].station;

    /* ---- Test 1 : broadcast (destination inconnue) ---- */
    printf("=== Test 1 : Envoi en broadcast (FF:FF:FF:FF:FF:FF) ===\n\n");

    MAC broadcast = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    uint8_t donnees[46] = "Bonjour tout le monde !";

    TrameEthernet trame1 = creer_trame(src->mac, broadcast, 0x0800,
                                        donnees, 46);
    afficher_trame(&trame1);
    afficher_trame_hex(&trame1);
    diffuser_trame(lan, &trame1, id_src);

    /* ---- Test 2 : unicast vers la 2ème station ---- */
    printf("=== Test 2 : Envoi unicast vers station %d ===\n\n", id_dst);

    memset(donnees, 0, sizeof(donnees));
    strncpy((char*)donnees, "Message direct", 46);

    TrameEthernet trame2 = creer_trame(src->mac, dst->mac, 0x0800,
                                        donnees, 46);
    afficher_trame(&trame2);
    diffuser_trame(lan, &trame2, id_src);

    /* ---- Test 3 : après apprentissage, le switch connaît la MAC source ---- */
    printf("=== Test 3 : Réponse de la station %d vers station %d ===\n\n",
           id_dst, id_src);

    TrameEthernet trame3 = creer_trame(dst->mac, src->mac, 0x0800,
                                        donnees, 46);
    afficher_trame(&trame3);
    diffuser_trame(lan, &trame3, id_dst);

    liberer_lan(lan);
}

/* Gestionnaire : EVENT_DEMO_ETAPE4 */
static void traiter_etape4(Evenement evt) {
    const char *fichier = evt.donnees.fichier;
    
    printf("╔════════════════════════════════════════╗\n");
    printf("║  ÉTAPE 4 : Spanning Tree Protocol      ║\n");
    printf("╚════════════════════════════════════════╝\n\n");

    LAN *lan = charger_lan(fichier);
    if (lan == NULL) return;

    /* Exécuter le protocole STP */
    executer_stp(lan);

    /* Afficher les résultats */
    afficher_resultats_stp(lan);

    liberer_lan(lan);
}

/* -----------------------------------------------------------------------
 * Dispatch des événements
 *
 * Fonction centrale qui appelle le gestionnaire approprié selon le type.
 * ----------------------------------------------------------------------- */

static void traiter_evenement(Evenement evt) {
    switch (evt.type) {
        case EVENT_DEMO_ETAPE1:
            traiter_etape1(evt);
            break;
        case EVENT_DEMO_ETAPE2:
            traiter_etape2(evt);
            break;
        case EVENT_DEMO_ETAPE3:
            traiter_etape3(evt);
            break;
        case EVENT_DEMO_ETAPE4:
            traiter_etape4(evt);
            break;
        case EVENT_QUIT:
            /* Signal d'arrêt, rien à faire */
            break;
        default:
            printf("Événement inconnu : %d\n", evt.type);
    }
}

/* -----------------------------------------------------------------------
 * Point d'entrée avec boucle d'événements
 *
 * La fonction main :
 *   1. Initialise la queue d'événements
 *   2. Enqueue les 4 étapes à exécuter
 *   3. Lance la boucle d'événements pour traiter chaque événement
 *   4. Nettoie les ressources
 * ----------------------------------------------------------------------- */

int main(int argc, char *argv[]) {
    /* Fichier de configuration à utiliser */
    const char *fichier = (argc > 1) ? argv[1] : "configs/configs/config2.txt";

    printf("\n");
    printf("████████████████████████████████████████████\n");
    printf("  SAE 2.3 - Simulation de réseau local\n");
    printf("  Architecture : Événementielle (Event-driven)\n");
    printf("████████████████████████████████████████████\n\n");

    /* -----------------------------------------------------------------------
     * Initialisation : création de la queue d'événements
     * ----------------------------------------------------------------------- */
    
    QueueEvenement *queue = creer_queue_evenement(10);
    if (queue == NULL) {
        fprintf(stderr, "Erreur : impossible de créer la queue d'événements.\n");
        return 1;
    }

    /* -----------------------------------------------------------------------
     * Enqueuing : ajouter les 4 étapes à la queue
     *
     * Priorité : 0 = haute (exécuté en premier)
     *           1, 2, 3 = basse (exécuté après)
     *
     * Les étapes sont enqueued dans l'ordre, mais elles pourraient
     * être exécutées dans un ordre différent si les priorités étaient différentes.
     * ----------------------------------------------------------------------- */

    printf("Initialisation des événements...\n");

    Evenement evt1 = { EVENT_DEMO_ETAPE1, {0}, 0 };
    enqueuer_evenement(queue, evt1);
    printf("   ✓ Étape 1 enqueued\n");

    Evenement evt2 = { EVENT_DEMO_ETAPE2, {(char*)fichier}, 1 };
    enqueuer_evenement(queue, evt2);
    printf("   ✓ Étape 2 enqueued\n");

    Evenement evt3 = { EVENT_DEMO_ETAPE3, {(char*)fichier}, 2 };
    enqueuer_evenement(queue, evt3);
    printf("   ✓ Étape 3 enqueued\n");

    Evenement evt4 = { EVENT_DEMO_ETAPE4, {(char*)fichier}, 3 };
    enqueuer_evenement(queue, evt4);
    printf("   ✓ Étape 4 enqueued\n");

    printf("\n");

    /* -----------------------------------------------------------------------
     * BOUCLE D'ÉVÉNEMENTS : traiter les événements un par un
     *
     * C'est le cœur du pattern événementiel :
     * tant qu'il y a des événements en queue, on les traite dans l'ordre
     * de priorité.
     * ----------------------------------------------------------------------- */

    printf("Démarrage de la boucle d'événements...\n\n");

    while (!queue_vide(queue)) {
        Evenement evt = dequeur_evenement(queue);
        
        /* Afficher un séparateur avant chaque événement (sauf le premier) */
        if (evt.type != EVENT_DEMO_ETAPE1) {
            printf("────────────────────────────────────────────\n\n");
        }

        /* Traiter l'événement */
        traiter_evenement(evt);
    }

    /* -----------------------------------------------------------------------
     * Nettoyage
     * ----------------------------------------------------------------------- */

    liberer_queue_evenement(queue);

    printf("────────────────────────────────────────────\n");
    printf("Boucle d'événements terminée.\n");
    printf("   Toutes les étapes ont été exécutées.\n\n");

    return 0;
}
