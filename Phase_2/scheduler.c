#ifndef SCHEDULER_C 
#define SCHEDULER_C
#include <initial.c>

extern void *memcpy(void *dest, const void *src, unsigned int n);


void scheduling(){
    while (true) {
        /* readyQ non vuota */
        if (!emptyProcQ(&readyQ))
        {
            current_process = removeProcQ(&readyQ);
            setTIMER(TIMESLICE);

            STCK(current_process->istante_Lancio_Blocco); 
            /* per salvarsi il momento in cui viene lanciato */
            
            LDST(&current_process->p_s);
        }
        else if(process_count==0)
        {
            HALT();
        }
        else if(process_count>0 && soft_block_count>0)
        {
            /* the Scheduler must first set the Status register
             to enable interrupts and either disable the PLT
            The first interrupt that occurs after entering a 
            Wait State should not be for the PLT. */
            setSTATUS((getSTATUS() ^ TEBITON) | IEPON);
            /* fare qualcosa con il PLT*/
            WAIT();
        }
        else if (process_count>0 && soft_block_count==0)
        {
            PANIC();
        }
        
    }
}
#endif
