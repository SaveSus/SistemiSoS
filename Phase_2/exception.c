#include <umps3/umps/libumps.h>
#include <umps3/umps/types.h>
#include <pcb.h>
#include <ash.h>
#include <ns.h>
#include <scheduler.c>
#include <initial.c>
#include <pandos_const.h>
#include <umps3/umps/const.h>

/* come process id usiamo un intero che aumenta 
    e basta (no caso reincarazione)*/
int pid_start = 0;

void uTLB_RefillHandler () 
{
    setENTRYHI(0x80000000);
    setENTRYLO(0x00000000);
    TLBWR();
    LDST((state_t *)0x0FFFF000);
}

/* CONTROLLARE LA SEZIONE 3.5.12 */
void foobar() 
{   
    current_process -> p_s = (state_t*) BIOS_DATA_PAGE;
    /* fornisce il codice del tipo di eccezione avvenuta */
    switch (CAUSE_GET_EXCCODE((int)current_process->p_s.cause))
    {
    case 0:
        interrupt_handler();
        break;
    case 1: case 2: case: 3
        passup_ordie(PGFAULTEXCEPT);
        break;
    case 4: case 5: case 6: case 7:
        passup_ordie(GENERALEXCEPT);
        break;
    case 9: case 10: case 11: case 12:
        passup_ordie(GENERALEXCEPT);
        break;
    case 8:
        syscall_handler();
    default:
        break;
    }
}

/*siccome perTLB , ProgramTrap e SYSCALL > 11 bisogna effettuare PASS UP OR DIE avrebbe senso creare una funzione*/
//DA RIGUARDARE 3.7 
void passup_ordie(int INDEX) {
    if (current_process->p_supportStruct == NULL) {
        SYS_terminate_process(0);
    }
    else {
        context_t exceptContext = current_process->p_supportStruct->sup_exceptContext[INDEX];
        current_process->p_supportStruct->sup_exceptState[INDEX].status  = (state_t*)BIOS_DATA_PAGE;
        LDCXT(exceptContext.stackPtr,exceptContext.status,exceptContext.pc);
    }
}

/* Per le sys 3, 5, 7 servono delle operazioni in più, sezione 3.5.13 */
void syscall_handler() {
    switch ((int)current_process->p_s.reg_a0)
    {
    case CREATEPROCESS:
        SYS_create_process((state_t*)current_process->p_s.reg_a1, reg_a2, reg_a3);
        break;
        
    case TERMPROCESS:
        SYS_terminate_process((int)current_process->p_s.reg_a1);
        break;
    
    case PASSEREN:
        SYS_Passeren((int*)current_process->p_s.reg_a1);
        break;
        
    case VERHOGEN:
        SYS_Verhogen((int*)current_process->p_s.reg_a1);
        break;
    
    case DOIO:
        SYS_Doio((int*)current_process->p_s.reg_a1, (int*)current_process->p_s.reg_a2)
        break;
        
    case GETTIME:
        SYS_Get_CPU_Time();
        break;

    case CLOCKWAIT:
        SYS_Clockwait();
        break;
    
    case GETSUPPORTPTR:
        SYS_Get_Support_Data();
        break;
        
    case GETPROCESSID:
        SYS_Get_Process_Id((int)current_process->p_s.reg_a1);
        break;

    case: GETCHILDREN:
        SYS_Get_Children((int*)current_process->p_s.reg_a1, (int)current_process->p_s.reg_a2);
    default:
        break;
    }
}

/* Crea un nuovo processo come figlio del chiamante. Il primo parametro contiene lo stato
 che deve avere il processo. Se la system call ha successo il valore di ritorno (nel registro reg_v0)
 è il pid creato altrimenti è -1. supportp e’ un puntatore alla struttura di supporto del processo.
 Ns descrive il namespace di un determinato tipo da associare al processo, senza specificare
il namespace (NULL) verra’ ereditato quello del padre.*/
void SYS_create_process(state_t *statep, support_t *supportp, nsd_t *ns)
{
    pcb_t *newProc = allocPcb();

    if (newProc!=NULL) {
        /* newProc sarà il figlio di current_process e sarà disponibile nella readyQ*/
        insertChild(&current_process, &newProc);
        insertProcQ(&readyQ, newProc);
        newProc->p_s = *statep;
        newProc->p_supportStruct = supportp;

        if (!addNamespace(newProc, ns)) { /* deve ereditare il ns dal padre */
            newProc->namespaces = current_process->namespace;
        }

        newProc->p_pid = pid_start + 1; /* assegniamo il pid */
        newProc->p_time = 0;

        process_count=+1; /* stiamo aggiunge un nuovo processo tra quelli attivi ? */

        current_process->p_s.reg_v0 = newProc->p_pid;
    }
    else { /* non ci sono pcb liberi */
        current_process->p_s.reg_v0 = -1;
    }
}


/* Termina il processo con identificativo pid e tutti suoi figli
 (e figli dei figli...) se pid è 0 allora termina il processo corrente */
void SYS_terminate_process(int pid)
{
    pcb_t *Proc2Delete;
    if(pid == 0){
        Proc2Delete = current_process;
    } else{
        for(int i=0; i<MAXPROC; i++) {
            if(pcbFree_table[i]->p_pid == pid){
                Proc2Delete = &pcbFree_table[i];
            }
        }
    }

    terminate_family(Proc2Delete);
}

/*Uccide un processo e tutta la sua progenie (NON I FRATELLI DEL PROCESSO CHIAMATO) */
void terminate_family(pcb_t *ptrn)
{
    /* se ha dei figli richiama la funzione stessa */
    if(!emptyChild(ptrn)) {        
        struct list_head *pos, *current = NULL;
        list_for_each_safe(pos, current, ptrn->p_child->p_sib) {
            pcb_t* temp = list_entry(pos, struct pcb_t, p_sib);
            terminate_family(temp);
        }  
    }

    /* richiamo la funzione per i fratelli di ptrn */
    
    kill_process(ptnr);
    /* penso che dobbiamo controllare se tra i processi che eliminiamo
     ci sono dei processi bloccati e in quel caso diminuire il soft_block_count */
}

void kill_process(pcb_t* ptnr)
{
    Outchild(ptnr);
    /* uccido ptrn */
    ptnr->p_parent = NULL;
    list_del(&ptrn->p_list);
    list_del(&ptrn->p_child);
    list_del(&ptrn->p_sib);
    /* forse dobbiamo eliminare il processo dalla ahs (?) */
    if(ptrn->p_semAdd != NULL){

        ptrn->p_semAdd = NULL;
    }
    ptrn->p_pid = 0;
    freePcb(ptrn);

    process_count--;
}


/* Operazione di richiesta di un semaforo binario.
 Il valore del semaforo è memorizzato nella variabile di tipo intero passata per indirizzo. 
 L’indirizzo della variabile agisce da identificatore per il semaforo */
void SYS_Passeren(int *semaddr) 
{
    /* dobbiamo usare la hash dei semafori attivi */
    int pid_current = current_process->p_pid;
    if(*semaddr==0) {
        /* aggiungere current_process nella coda dei 
         processi bloccati da una P e sospenderlo*/
        int inserimento_avvenuto = insertBlocked(semaddr, current_process);
        /* se inserimento_avvenuto è 1 allora non è stato possibile allocare un nuovo SEMD perché la semdFree_h è vuota */

        /* chiamata allo scheduler, non so si può far direttamente così */
        scheduling();
    } else if( /* se la coda dei processi bloccati da V non è vuota*/headBlocked(semaddr)!=NULL) {
        /* risvegliare il primo processo che si era bloccato su una V */
        pcb_t* wakedProc = removeBlocked(semaddr);
        LDST(&wakedeProc->p_s);
    } else {
        semaddr--;
    }
}

/* Operazione di rilascio di un semaforo binario la cui chiave è il valore puntato da semaddr */
void SYS_Verhogen(int* semaddr)
{
    int pid_current = current_process->p_pid;
    if(*semaddr==1) {
        /* aggiungere current_process nella coda dei 
         processi bloccati da una V e sospenderlo*/
        int inserimento_avvenuto = insertBlocked(semaddr, current_process);
        /* se inserimento_avvenuto è 1 allora non è stato possibile allocare un nuovo SEMD perché la semdFree_h è vuota */

         /* chiamata allo scheduler, non so si può far direttamente così */
        scheduling();
    } else if( /* se la coda dei processi bloccati da P non è vuota*/headBlocked(semaddr)!=NULL) {
        /* risvegliare il primo processo che si era bloccato su una P */
        pcb_t* wakedProc = removeBlocked(semaddr);
        LDST(&wakeProc->p_s);
    } else {
        semaddr++;
    }
}


/* Effettua un’operazione di I/O. CmdValues e’ un vettore di 2 interi
 (per I terminali) o 4 interi (altri device).
– La system call carica I registri di device con i valori di CmdValues
    scrivendo il comando cmdValue nei registri cmdAddr e seguenti, 
    e mette in pausa il processo chiamante fino a quando non si e’ conclusa.
– L’operazione è bloccante, quindi il chiamante viene sospeso sino 
    alla conclusione del comando. Il valore ritornato deve essere 
    zero se ha successo, -1 in caso di errore. Il contenuto del registro di
    status del dispositivo potra’ essere letto nel corrispondente elemento 
    di cmdValues.
At the completion of the I-O operation the device register values are
copied back in the cmdValues array
*/
SYS_Doio(int*comdAddr, int*comdValues)
{

}

/* Restituisce il tempo di utilizzo del processore del processo in esecuzione*/
void SYS_Get_CPU_Time()
{
    /* Hence SYS6 should return the value in the Current Process’s p_time PLUS
     the amount of CPU time used during the current quantum/time slice.*/
    current_process->p_s.reg_v0 = current_process->p_time;
}

/* 
Equivalente a una Passeren sul semaforo dell’Interval Timer.
– Blocca il processo invocante fino al prossimo tick del dispositivo.
*/
SYS_Clockwait()
{
    
}

/* Restituisce un puntatore alla struttura di supporto del processo corrente,
 ovvero il campo p_supportStruct del pcb_t.*/
void SYS_Get_Support_Data()
{
    current_process->p_s.reg_v0 = current_process->p_supportStruct;
}

/* Restituisce l’identificatore del processo invocante se parent == 0,
 quello del genitore del processo invocante altrimenti.
 Se il parent non e’ nello stesso PID namespace del processo figlio,
 questa funzione ritorna 0 (se richiesto il pid del padre)! */
void SYS_Get_Process_Id(int parent)
{
    if (parent == 0) {
        current_process->p_s.reg_v0 = current_process->p_pid;
    } 
    else { /* dobbiamo restituire il pid del padre, se si trovano nello stesso namespace */
        /* assumiamo che il processo corrente abbia un padre (?) */
        nsd_t* parent_pid = getNamespace(current_process->p_parent, current_process->namespaces.n_type);
        
        /* se current_process e il processo padre non sono nello stesso namespace restituisci 0 */ 
        current_process->p_s.reg_v0 = (parent_pid==NULL) ? 0 : current_process->p_pid;
    }
}

/**/
void SYS_Get_Children(int *children, int size){
    int num = 0;
    struct list_head *pos, *current = NULL;
    int current_namespace = current_process->namespaces[0].n_type
    pcb_t *first_child = current_process->p_child;
    list_for_each_safe(pos, current, first_child->p_sib){
        pcb_t* temp = list_entry(pos, struct pcb_t, p_sib);
        if(current_namespace == temp->namespaces[0].n_type){
            if(num < size){
                children[num] = temp->p_pid;
            }
            num++;
        }   
    }
    reg_v0 = num;
}

/* Per determinare se il processo corrente stava eseguento in kernel o user mode,
 dobbiamo esaminare lo Status register in the saved exception state. 
 In particolare, dobbiamo esaminare la versione precedente del KU bit (KUp) siccome 
 sarà avvenuta una stack push sul KU/IE stacks in the statusa register prima 
 che lo stato di eccezione fosse salvato*/
int Check_Kernel_mode()
{ 
    unsigned mask;
    mask = ((1 << 1) - 1) << STATUS_KUp_BIT;
    unsigned int bit_kernel = current_process->p_s.status & mask;
    /* ritorna vero se il processo era in kernel mode, 0 in user mode*/
    return (bit_kernel==0) ? TRUE : FALSE;
}

/* Restituisce la linea con interrupt in attesa con massima priorità. 
(Se nessuna linea è attiva ritorna 8 ma assumiamo che quando venga
 chiamata ci sia almeno una linea attiva) */
int Get_Interrupt_Line_Max_Prio (){
    unsigned int interrupt_pendig = current_process->p_s.cause & CAUSE_IP_MASK;
    /* così abbiamo solo i bit attivi da 8 a 15 del cause register */
    unsigned int intpeg_linee[8];
    for (int i=0; i<8; i++) {
        unsigned mask = ((1<<1)-1)<<i+8;
        intpeg_linee[i] = mask & interrupt_pending;
    }
    /* intpeg_linee[i] indica se la linea i-esima è attiva */
    
    int linea=1; /* questo perché ignoriamo la linea 0*/
    while(linea<8) {
        if(intpeg[linea]!=0) { 
            break;
        }
        linea++;
    }
    /* ritorniamo la quale linea è attiva */
    return linea;
}

/*
The interrupt exception handler’s first step is to determine which device
 or timer with an outstanding interrupt is the highest priority.
 Depending on the device, the interrupt exception handler will 
 perform a number of tasks.*/
void interrupt_handler()
{
    /* sezione 3.6.1 a 3.6.3*/
    switch (Get_Interrupt_Line_Max_Prio())
    {
    /* interrupt processor Local Timer */
    case 1:
        PLT_interrupt_handler();
        break;
    
    /* interrupt Interval Timer */
    case 2:
       
        break;

    /* Disk devices */
    case DISKINT:
        
        break;
    
    /* Flash devices */
    case FLASHINT:
        
        break;
    
    /* Network devices*/
    case NETWINT:
        break;

    /* Printer devices */
    case PRNTINT:
        
        break;
    
    /* Terminal devices*/
    case TERMINT:
    
        break;

    default:
        break;
    }
   
}

//3.6.2
void PLT_interrupt_handler() {
    /*Acknowledge the PLT interrupt by loading the timer with a new value.*/
    setTIMER(500);

    /* Copy the processor state at the time of the exception (located at the start of the BIOS Data Page [Section ??-pops]) into the Current Pro- cess’s pcb (p_s). */
    // GIÀ FATTO (CREDO) facendolo all'inizio modifichiamo la variabile globale quindi ha senso farlo una sola volta all'inizio

    /* Place the Current Process on the Ready Queue; transitioning the Current Process from the “running” state to the “ready” state. */
    insertProcQ(&current_process->p_list, readyQ);
    /* credo bisogni diminuire questo counter*/
    process_count--;

    scheduling();
}