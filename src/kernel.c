#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>


#include "kernel.h"
#include "loadProgram.h"
#include "contextSwitch.h"
#include "traps.h"
#include "pcbManager.h"
#include "terminals.h"
#include <string.h>


#define PFN_TO_ADDR(pfn) (pfn << PAGESHIFT) // Definitely correct
#define ADDR_TO_PFN(addr) (addr >> PAGESHIFT)


/* ################### Global variables ################### */
InterruptP interrupt_vector_table[TRAP_VECTOR_SIZE];

Terminal terminals[NUM_TERMINALS];

int virtual_mem_enabled = 0; /* Is virtual memory enabled flag */


int *free_full_pages_arr;
int *free_half_pages_arr; // 0 means 0 1 means upper of 0, 2 means 1 etc
int ffpa_put_loc;
int fhpa_put_loc;   // note putloc = num available

void *kernel_brk;

struct pte *pt_r1;
struct pte *idle_pt_r0;

int PFN_CEILING = 0;

int haltable = 0;

PCB* current_process;

PCB *idle_proc_pcb;
PCB *init_proc_pcb;

unsigned int num_procs;


void addFullFreeFrame(unsigned int pfn) {
    free_full_pages_arr[ffpa_put_loc] = pfn;
    ffpa_put_loc++;    
}

unsigned int getFullFreeFrame() {
    ffpa_put_loc--;
    if (ffpa_put_loc < 0) {
        ffpa_put_loc = 0;
        return -1;
    }
    return free_full_pages_arr[ffpa_put_loc];
}

void addHalfFreeFrame(unsigned int pfn) {
    free_half_pages_arr[fhpa_put_loc] = pfn;
    fhpa_put_loc++;
}

unsigned int getHalfFreeFrame() {
    fhpa_put_loc--;
    if (fhpa_put_loc < 0) {
        fhpa_put_loc = 0;

        unsigned int pfn = getFullFreeFrame();
        if (pfn == (unsigned int) -1) {
            return -1;
        }
        addHalfFreeFrame(pfn * 2 + 1);
        TracePrintf(0, "returning half frame %d\n", pfn * 2);
        return pfn * 2;
    }
    TracePrintf(0, "returning half frame %d\n", free_half_pages_arr[fhpa_put_loc]);
    return free_half_pages_arr[fhpa_put_loc];
}

void KernelStart(ExceptionInfo *info, unsigned int pmem_size, void *orig_brk, char **cmd_args) {
    
    TracePrintf(MY_TRACE_LEVEL, "KernelStart: Start\n");
    
    kernel_brk = orig_brk;

    TracePrintf(0, "kbrk -10 %p\n", kernel_brk);

    //////// INITS ///////
    pcbManagerInit();
    //////////////////////
    
    TracePrintf(0, "kbrk -5 %p\n", kernel_brk);
    
    /* ################### init interrupt vector table initialization ################### */
    for (int i = 0; i < TRAP_VECTOR_SIZE; i++) {
        interrupt_vector_table[i] = NULL;
    }
    
    interrupt_vector_table[TRAP_KERNEL] = &TrapKernelHandler;
    interrupt_vector_table[TRAP_CLOCK] = &TrapClockHandler;
    interrupt_vector_table[TRAP_ILLEGAL] = &TrapIllegalHandler;
    interrupt_vector_table[TRAP_MEMORY] = &TrapMemoryHandler;
    interrupt_vector_table[TRAP_MATH] = &TrapMathHandler;
    interrupt_vector_table[TRAP_TTY_RECEIVE] = &TrapTTYReceiveHandler;
    interrupt_vector_table[TRAP_TTY_TRANSMIT] = &TrapTTYTransmitHandler;


    WriteRegister(REG_VECTOR_BASE, (RegVal) interrupt_vector_table);

    /* ################### Terminal Initializations ################### */
    for (int term = 0; term < NUM_TERMINALS; term++) {
        terminals[term].recieved_lines = SpawnQueue();
        terminals[term].PendingOutputsQueue = SpawnQueue();
        terminals[term].BlockedReadersQueue = SpawnQueue();
        terminals[term].outputPCB = NULL;
    }

	/* ################### establish mallocs ################### */
	pt_r1 = (struct pte *) ((uintptr_t) pmem_size + PMEM_BASE - PAGE_TABLE_SIZE);
    idle_pt_r0 = (struct pte *) ((uintptr_t) pmem_size + PMEM_BASE - (PAGE_TABLE_SIZE * 2));

    idle_proc_pcb = malloc(sizeof(PCB));

    idle_proc_pcb->ctx = malloc(sizeof(SavedContext));
    idle_proc_pcb->pt_r0 = (uintptr_t) idle_pt_r0;
    idle_proc_pcb->num_children = 0;
    idle_proc_pcb->clock_ticks = 0;
    idle_proc_pcb->brk = MEM_INVALID_PAGES;
    idle_proc_pcb->status_queue = SpawnQueue();
    idle_proc_pcb->pid = getPid();
    idle_proc_pcb->parent = NULL;
    idle_proc_pcb->user_stack_vpn_low = (VMEM_0_LIMIT >> PAGESHIFT) - KERNEL_STACK_PAGES - 2;

    free_full_pages_arr = malloc(sizeof(int) * (pmem_size >> PAGESHIFT));
    free_half_pages_arr = malloc(2 * sizeof(int) * (pmem_size >> PAGESHIFT));

    /* ################### set pt of r1 ################### */
   
    uintptr_t vmem_track = VMEM_1_BASE;
    
    while(vmem_track <= (uintptr_t) kernel_brk) {
        int idx = (vmem_track - VMEM_1_BASE) >> PAGESHIFT;
        pt_r1[idx].valid = 1;
        pt_r1[idx].pfn = vmem_track >> PAGESHIFT;
        pt_r1[idx].kprot = (vmem_track < (uintptr_t) &_etext) ? (PROT_READ | PROT_EXEC) : (PROT_READ | PROT_WRITE);
        pt_r1[idx].uprot = PROT_NONE;

        vmem_track += PAGESIZE;
    }

    pt_r1[PAGE_TABLE_LEN - 1].valid = 1;
    pt_r1[PAGE_TABLE_LEN - 1].pfn = (pmem_size >> PAGESHIFT) - 1;
    pt_r1[PAGE_TABLE_LEN - 1].kprot = (PROT_READ | PROT_WRITE);
    pt_r1[PAGE_TABLE_LEN - 1].uprot = PROT_NONE;

    WriteRegister(REG_PTR1, (RegVal) pt_r1);

    /* ################### set pt_r0 for idle ################### */

    vmem_track = KERNEL_STACK_BASE;
    while(vmem_track < KERNEL_STACK_LIMIT) {
        int idx = vmem_track >> PAGESHIFT;

        idle_pt_r0[idx].valid = 1;
        idle_pt_r0[idx].pfn = idx;
        idle_pt_r0[idx].kprot = PROT_READ | PROT_WRITE;
        idle_pt_r0[idx].uprot = PROT_NONE;

        vmem_track += PAGESIZE;
    }    

    WriteRegister(REG_PTR0, (RegVal) idle_proc_pcb->pt_r0);
    
    /* ################### init free frame queues ################### */  
    int pfn = 0;
    while (pfn < PAGE_TABLE_LEN - KERNEL_STACK_PAGES) {
        addFullFreeFrame(pfn);
        pfn++;
    }

    pfn = UP_TO_PAGE(kernel_brk) >> PAGESHIFT;
    while ((unsigned int) pfn < ((PMEM_BASE + pmem_size) >> PAGESHIFT) - 1) {  // LOOK OVER, is four ok?
        addFullFreeFrame(pfn);
        pfn++;
    }

    /* ################### enable vmem ###################*/
    TracePrintf(0, "ENABLE VMEM\n");
    WriteRegister(REG_VM_ENABLE, 1);    
    virtual_mem_enabled = 1;

    /* LOAD IDLE */

    init_proc_pcb = createPcb();

    current_process = idle_proc_pcb;
    LoadProgram(info, "./user_prog/idle", cmd_args);
    ContextSwitch(MySwitch, idle_proc_pcb->ctx, NULL, idle_proc_pcb);
    
    TracePrintf(MY_TRACE_LEVEL, "!!!!!!!!!!! KernelStart: IDLE LOADED\n");
    
    ContextSwitch(MySwitch, init_proc_pcb->ctx, idle_proc_pcb, init_proc_pcb);
    TracePrintf(MY_TRACE_LEVEL, "!!!!!!!!!!! KernelStart: POST FINAL CONTEXT SWITCH\n");
    
    if (current_process->pid == 0) {
        TracePrintf(MY_TRACE_LEVEL, "!!!!!!!!!!! KernelStart: End for idle\n");
        addReady(init_proc_pcb);
        // PrintPageTable(idle_proc_pcb->pt_r0);
    } else if (current_process->pid == 1) {

        if (cmd_args == NULL || cmd_args[0] == NULL) {
            TracePrintf(MY_TRACE_LEVEL, "SetKernelStar - loading init\n");
            int didFail = LoadProgram(info, "./tests/samples-ossim/init", cmd_args);
            if (didFail != 0) {
                Halt();
            }
            TracePrintf(MY_TRACE_LEVEL, "!!!!!!!!!!! KernelStart: INIT LOADED WITH BASICS\n");
            
        } else {
            int didFail = LoadProgram(info, cmd_args[0], cmd_args);
            if (didFail != 0) {
                Halt();
            }
            TracePrintf(MY_TRACE_LEVEL, "!!!!!!!!!!! KernelStart: INIT LOADED WITH COMMAND ARGS \n");
        }
        
        TracePrintf(MY_TRACE_LEVEL, "!!!!!!!!!!! KernelStart: End for init\n");

        // PrintPageTable(init_proc_pcb->pt_r0);
        num_procs = 1;
        haltable = 1;
        return;
    }
}

int SetKernelBrk(void *addr) {
    TracePrintf(MY_TRACE_LEVEL+1, "SetKernelBrk: start\n");
    if (!virtual_mem_enabled) {
        // Virtual memory not yet enabled -> physical address. Move up kernel_brk's pointer to addr. 
        if ((uintptr_t)addr > VMEM_1_LIMIT || addr < VMEM_1_BASE || addr < kernel_brk) {
            TracePrintf(MY_TRACE_LEVEL+1, "SetKernelBrk: fail\n");
            return -1;
        }
        kernel_brk = addr;
        TracePrintf(MY_TRACE_LEVEL+1, "SetKernelBrk: end, pre vmem\n");
        return 0;
    } else {
        unsigned long new_address, vpn_idx;
        if ((unsigned long) ((unsigned long) addr - (unsigned long) UP_TO_PAGE(kernel_brk)) > (unsigned long)(PAGESIZE * ffpa_put_loc)){
            return -1;
        }

        struct pte *pt_r1_ref = reference_pt_r0((uintptr_t) pt_r1);

        for (new_address = UP_TO_PAGE(kernel_brk) - 1; new_address < (unsigned long)addr; new_address += PAGESIZE) {
            vpn_idx = (new_address - VMEM_1_BASE) >> PAGESHIFT;
            if (pt_r1_ref[vpn_idx].valid == 0) {
                unsigned int pfn = getFullFreeFrame();
                if (pfn == (unsigned int)-1) {
                    pt_r1_ref[vpn_idx].valid = 0; // Ensure valid bit is not set
                    KernelExit(ERROR);
                }
                pt_r1_ref[vpn_idx].pfn = pfn;
                pt_r1_ref[vpn_idx].valid = 1;
                pt_r1_ref[vpn_idx].kprot = PROT_READ | PROT_WRITE;
                pt_r1_ref[vpn_idx].uprot = PROT_NONE;
            }
        }

        dereference_pt_r0(pt_r1_ref);
        WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
    }
    
    TracePrintf(MY_TRACE_LEVEL+1, "SetKernelBrk: end\n");
    return 0;
}

