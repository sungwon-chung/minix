#ifndef _PCBMANAGER_H
#define _PCBMANAGER_H

#include "queue.h"
#include <hwsim/hardware.h>
#include <stdint.h>

typedef struct pcb {
    SavedContext *ctx; // Where copy of the current CPU state (i.e. registers) are saved.
    uintptr_t pt_r0;    // PTRO to context switch to Region 0 of process's (physical address).
    int num_children;   // Number of child processess associated with this process.
    int clock_ticks; // Each process runs for at least 2 clock ticks if there are other processes on the readyQ.
    uintptr_t brk;  // address of the end of the process's data segment.
    Queue *status_queue;    //FIFO Queue of child processes not yet collected by parent
    unsigned int pid;    // Process ID.
    struct pcb *parent;  // pointer to pointer of parent pcb
    int dead; // 1 means dead
    int user_stack_vpn_low; // VPN of the low end of the user stack
    Queue *children; // queue of children procs
} PCB;

typedef struct status_queue_data {
    unsigned int pid;
    int exit_status;
} SQ_DATA;

extern Queue *readyQueue;
extern Queue *blockedQueue;
extern Queue *waitQueue;

extern int pcbManagerInit();
extern PCB* createPcb();
extern void destroyPcb(PCB *p);
extern PCB* getNextReady();
extern void addReady(PCB *p);
extern void* reference_pt_r0(uintptr_t loc);
extern void dereference_pt_r0(void * ptr);
extern unsigned int getPid();

#endif