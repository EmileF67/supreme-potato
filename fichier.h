#ifndef FICHIER_H
#define FICHIER_H

#include "types.h"

/*
 * Charge un réseau local depuis un fichier de configuration.
 * Format attendu :
 *   - 1ère ligne : nb_équipements nb_liens
 *   - Lignes équipements : "2;MAC;ports;priorité" (switch) ou "1;MAC;IP" (station)
 *   - Lignes liens : "eq1;eq2;coût"
 *
 * Retourne un pointeur vers le LAN alloué, ou NULL en cas d'erreur.
 * Vous devez appeler liberer_lan() quand vous n'en avez plus besoin.
 */
LAN *charger_lan(const char *chemin_fichier);

/* Libère toute la mémoire allouée par un LAN */
void liberer_lan(LAN *lan);

/*
 * Sauvegarde un réseau local dans un fichier de configuration
 * au même format que charger_lan().
 * Retourne 0 en cas de succès, -1 en cas d'erreur.
 */
int sauvegarder_lan(LAN *lan, const char *chemin_fichier);

#endif
