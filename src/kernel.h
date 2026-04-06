#ifndef _KERNEL_H
#define _KERNEL_H

#include <hwsim/hardware.h>
#include <hwsim/minix.h>
#include <stdlib.h>
#include <stdint.h>
#include "queue.h"
#include "pcbManager.h"
#include "calls.h"
#include "terminals.h"

#define MY_TRACE_LEVEL 7
#define MODERATE_TRACE_LEVEL 6
#define HIGH_TRACE_LEVEL 8



extern void logger(ExceptionInfo *info);

extern SavedContext *current_info;

typedef void (*InterruptP) (ExceptionInfo *); /* typedef alias for the function pointer type */

extern InterruptP interrupt_vector_table[TRAP_VECTOR_SIZE]; /* Interrupt vector table */
extern void *kernel_brk; /* Kernel Brk pointer */
extern struct pte *pt_r1;
extern PCB *current_process;
extern PCB *idle_proc_pcb;
extern PCB *init_proc_pcb;


extern int *free_full_pages_arr;
extern int *free_half_pages_arr; // 0 means 0 1 means upper of 0, 2 means 1 etc
extern int ffpage_count;
extern int fhpage_count;
extern int ffpa_put_loc;
extern int fhpa_put_loc;

extern int haltable;

extern unsigned int num_procs;

extern void addFullFreeFrame(unsigned int page);
extern unsigned int getFullFreeFrame();

extern void addHalfFreeFrame(unsigned int page);
extern unsigned int getHalfFreeFrame();


extern Terminal terminals[NUM_TERMINALS]; // All minix terminals: 0...3.

#endif