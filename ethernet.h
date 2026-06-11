#ifndef ETHERNET_H
#define ETHERNET_H

#include "types.h"

/*
 * Crée une trame Ethernet avec les paramètres donnés.
 * Les champs préambule, SFD et FCS sont remplis automatiquement.
 * Les données doivent faire entre 46 et 1500 octets.
 */
TrameEthernet creer_trame(MAC source, MAC destination, uint16_t type,
                          uint8_t *donnees, int longueur);

/*
 * Simule la diffusion (broadcast) d'une trame dans le réseau.
 *
 * Lorsqu'une station envoie une trame, le switch qui la reçoit :
 *   1. Apprend l'adresse MAC source et le port d'entrée (MAJ table de commutation)
 *   2. Si la destination est connue dans sa table : envoie sur ce port uniquement
 *   3. Sinon : diffuse sur tous les ports actifs (sauf celui d'entrée)
 *
 * Cette fonction affiche pas à pas ce qui se passe dans le réseau.
 *
 * Paramètres :
 *   - lan        : le réseau local
 *   - trame      : la trame à envoyer
 *   - id_source  : indice de l'équipement qui envoie la trame
 */
void diffuser_trame(LAN *lan, TrameEthernet *trame, int id_source);

/*
 * Retourne l'indice du port d'un switch connecté à un équipement donné.
 * Retourne -1 si non trouvé.
 */
int trouver_port_switch(LAN *lan, int id_switch, int id_voisin);

#endif
