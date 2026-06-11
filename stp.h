#ifndef STP_H
#define STP_H

#include "types.h"

/*
 * Exécute le protocole STP (Spanning Tree Protocol) sur un réseau local.
 *
 * À la fin de l'exécution, chaque port de chaque switch aura un état :
 *   - PORT_RACINE   : port qui mène vers la racine (1 par switch sauf la racine)
 *   - PORT_DESIGNE  : port actif (transmet les données)
 *   - PORT_BLOQUE   : port désactivé (évite les boucles)
 *
 * L'algorithme s'exécute par rounds successifs jusqu'à convergence
 * (plus aucun changement d'un round à l'autre).
 */
void executer_stp(LAN *lan);

#endif
