#ifndef ASH_H
#define ASH_H

#include "pandos_types.h"
#include "pandos_const.h"
#include "hashtable.h"
#include "pcb.h"

HIDDEN semd_t semd_table[MAXPROC]; 
HIDDEN LIST_HEAD(semdFree_h);
HIDDEN DECLARE_HASHTABLE(semd_h, 5);

/* Inizializza la lista dei semdFree_h in modo da contenere tutti gli elementi della semdTable */
void initASH();

/**
 * Viene inserito il PCB puntato da p nella coda dei processi bloccati associata al SEMD con chiave semAdd. 
 * Se il semaforo corrispondente non è presente nella ASH, alloca un nuovo SEMD dalla lista di quelli liberi 
 * (semdFree) e lo inserisce nella ASH, settando I campi in maniera opportuna (i.e. key e s_procQ).
 * Se non è possibile allocare un nuovo SEMD perché la lista di quelli liberi è vuota, restituisce TRUE. 
 * In tutti gli altri casi, restituisce FALSE
 **/
int insertBlocked(int*, pcb_t*);

/** 
 * Rimuove il PCB puntato da p dalla coda del semaforo su cui è bloccato (indicato da p->p_semAdd). 
 * Se il PCB non compare in tale coda, allora restituisce NULL (condizione di errore). 
 * Altrimenti, restituisce p. Se la coda dei processi bloccati per il semaforo diventa vuota,
 * rimuove il descrittore corrispondente dalla ASH e lo inserisce nella coda dei descrittori liberi
 **/
pcb_t* outBlocked(pcb_t *);

/** 
 * Ritorna il primo PCB dalla coda dei processi bloccati (s_procq) associata al SEMD della ASH 
 * con chiave semAdd. Se tale descrittore non esiste nella ASH, restituisce NULL. Altrimenti, 
 * restituisce l’elemento rimosso. Se la coda dei processi bloccati per il semaforo diventa vuota,
 * rimuove il descrittore corrispondente dalla ASH e lo inserisce nella coda dei descrittori liberi (semdFree_h)
 **/
pcb_t* removeBlocked(int *);

/**
 * Restituisce (senza rimuovere) il puntatore al PCB che si trova in testa alla coda dei processi
 * associata al SEMD con chiave semAdd. Ritorna NULL se il SEMD non compare nella ASH oppure se 
 * compare ma la sua coda dei processi è vuota 
 **/
pcb_t* headBlocked(int *);

#endif