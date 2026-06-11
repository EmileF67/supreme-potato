/*
 * types.h - Définition de toutes les structures de données du projet
 *
 * Ce fichier contient les "moules" (structures) qui décrivent :
 *   - Une adresse MAC (identifiant matériel d'une carte réseau)
 *   - Une adresse IPv4 (adresse réseau logique)
 *   - Une station (un ordinateur dans le réseau)
 *   - Un switch (un équipement d'interconnexion)
 *   - Un LAN (le réseau local complet)
 *   - Une trame Ethernet (un paquet de données qui circule sur le réseau)
 */

#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>  /* pour uint8_t, uint16_t (entiers de taille fixe) */

/* -----------------------------------------------------------------------
 * Adresses réseau
 * ----------------------------------------------------------------------- */

/* Adresse MAC : 6 octets (ex: 01:45:23:a6:f7:ab) */
typedef struct {
    uint8_t octets[6];
} MAC;

/* Adresse IPv4 : 4 octets (ex: 130.79.80.21) */
typedef struct {
    uint8_t octets[4];
} IPv4;

/* -----------------------------------------------------------------------
 * Équipements réseau
 * ----------------------------------------------------------------------- */

/* Une station = un ordinateur (adresse MAC + adresse IP) */
typedef struct {
    MAC mac;
    IPv4 ip;
} Station;

/*
 * Entrée dans la table de commutation d'un switch :
 * "Pour l'adresse MAC X, utiliser le port Y"
 */
typedef struct {
    MAC mac;   /* adresse MAC connue */
    int port;  /* port par lequel cette adresse est accessible */
} EntreeTable;

/* État possible d'un port de switch (utilisé par STP) */
typedef enum {
    PORT_INCONNU  = 0,  /* état initial, pas encore configuré */
    PORT_RACINE   = 1,  /* port qui mène vers la racine STP */
    PORT_DESIGNE  = 2,  /* port actif qui transmet les données */
    PORT_BLOQUE   = 3   /* port désactivé pour éviter les boucles */
} EtatPort;

/* Un switch = équipement réseau (MAC + ports + priorité + table de commutation) */
typedef struct {
    MAC     mac;
    int     nb_ports;       /* nombre de ports physiques */
    int     priorite;       /* priorité STP (plus petit = plus prioritaire) */

    /* Table de commutation : liste des MAC connues et leurs ports */
    EntreeTable table[1024];
    int         nb_entrees;

    /* État de chaque port (alloué dynamiquement selon nb_ports) */
    EtatPort *etats_ports;
} Switch;

/* -----------------------------------------------------------------------
 * Structure du réseau local (LAN = graphe)
 * ----------------------------------------------------------------------- */

/* Type d'un équipement dans le réseau */
typedef enum {
    TYPE_SWITCH  = 2,
    TYPE_STATION = 1
} TypeEquipement;

/* Un équipement générique (soit une station, soit un switch) */
typedef struct {
    TypeEquipement type;
    union {
        Station station;
        Switch  sw;
    };
} Equipement;

/* Un lien entre deux équipements (une connexion réseau) */
typedef struct {
    int eq1;    /* indice du premier équipement */
    int eq2;    /* indice du deuxième équipement */
    int poids;  /* coût du lien (dépend du débit : 100Mb=19, 1Gb=4) */
} Lien;

/* Le réseau local complet : liste d'équipements + liste de liens */
typedef struct {
    int          nb_equipements;
    int          nb_liens;
    Equipement  *equipements;   /* tableau dynamique d'équipements */
    Lien        *liens;         /* tableau dynamique de liens */
} LAN;

/* -----------------------------------------------------------------------
 * Trame Ethernet (format exact conforme à la norme)
 * ----------------------------------------------------------------------- */

/*
 * Structure d'une trame Ethernet (conforme à la norme) :
 * préambule(7) | SFD(1) | dest(6) | src(6) | type(2) | données(0-1500) | bourrage(0-46) | FCS(4)
 *
 * Note : données + bourrage forment ensemble les "DATA" de 46 à 1500 octets minimum.
 *        Le bourrage sert à atteindre le minimum de 46 octets si les données sont trop courtes.
 */
typedef struct {
    uint8_t  preambule[7];   /* 0xAA x7 (synchronisation) */
    uint8_t  sfd;            /* 0xAB (Start Frame Delimiter) */
    MAC      destination;    /* adresse MAC destination (6 octets) */
    MAC      source;         /* adresse MAC source (6 octets) */
    uint16_t type;           /* protocole (0x0800=IPv4, 0x0806=ARP, 0x86DD=IPv6) */
    uint8_t  donnees[1500];  /* charge utile (0 à 1500 octets) */
    int      longueur_donnees; /* longueur réelle des données */
    uint8_t  bourrage[46];   /* padding pour atteindre 46 octets minimum (0 à 46 octets) */
    int      longueur_bourrage; /* longueur réelle du bourrage */
    uint8_t  fcs[4];         /* Frame Check Sequence (détection d'erreurs, 4 octets) */
} TrameEthernet;

/* -----------------------------------------------------------------------
 * BPDU (Bridge Protocol Data Unit) - message échangé par le protocole STP
 * ----------------------------------------------------------------------- */

/*
 * Un BPDU contient :
 *   - l'identifiant du switch racine connu
 *   - le coût (distance) depuis l'émetteur jusqu'à la racine
 *   - l'identifiant du switch qui envoie ce BPDU
 */
typedef struct {
    int  priorite_racine;   /* priorité du switch racine */
    MAC  mac_racine;        /* MAC du switch racine */
    int  cout;              /* coût accumulé jusqu'à la racine */
    int  priorite_emetteur; /* priorité du switch émetteur */
    MAC  mac_emetteur;      /* MAC du switch émetteur */
} BPDU;

#endif /* TYPES_H */
