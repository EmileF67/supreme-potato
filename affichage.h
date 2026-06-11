#ifndef AFFICHAGE_H
#define AFFICHAGE_H

#include "types.h"

/* Affiche une adresse MAC au format hexadécimal (ex: 01:45:23:a6:f7:ab) */
void afficher_mac(MAC mac);

/* Affiche une adresse IPv4 en notation décimale pointée (ex: 130.79.80.21) */
void afficher_ip(IPv4 ip);

/* Affiche les informations d'une station */
void afficher_station(Station *s, int indice);

/* Affiche les informations d'un switch (avec sa table de commutation) */
void afficher_switch(Switch *sw, int indice);

/* Affiche tous les équipements et liens d'un réseau local */
void afficher_lan(LAN *lan);

/* Affiche une trame Ethernet en mode "utilisateur" (lisible) */
void afficher_trame(TrameEthernet *t);

/* Affiche une trame Ethernet en hexadécimal (octets bruts) */
void afficher_trame_hex(TrameEthernet *t);

/* Affiche l'état des ports de tous les switchs après STP */
void afficher_resultats_stp(LAN *lan);

#endif
