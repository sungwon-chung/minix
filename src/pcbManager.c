#include "pcbManager.h"
#include "kernel.h"

unsigned int avail_pid = 0;
unsigned int getPid() {
    return avail_pid++;
}

Queue *readyQueue;
Queue *blockedQueue;
Queue *waitQueue;

int pcbManagerInit() {
    TracePrintf(MY_TRACE_LEVEL, "pcbManagerInit called\n");

    readyQueue = SpawnQueue();
    blockedQueue = SpawnQueue();
    waitQueue = SpawnQueue();

    if (!readyQueue || !blockedQueue) {
        TracePrintf(MY_TRACE_LEVEL, "pcbManagerInit FAILED\n");
        return -1;        
    }
    return 0;
}

PCB* createPcb() {
    TracePrintf(MY_TRACE_LEVEL, "createPcb called\n");
    struct pcb *proc = (struct pcb*)malloc(sizeof(struct pcb));

    

    unsigned int half_frame = getHalfFreeFrame();    
    unsigned int offset = (half_frame % 2) * (PAGESIZE >> 1);
    half_frame = half_frame >> 1;
    TracePrintf(MY_TRACE_LEVEL, "half frame pfn is %u   and offset is %u\n", half_frame, offset / (PAGESIZE >> 1));

    proc->ctx = (SavedContext*)malloc(sizeof(SavedContext));
    proc->pt_r0 = (uintptr_t) (half_frame << PAGESHIFT) + offset + VMEM_0_BASE;
    proc->num_children = 0;
    proc->clock_ticks = 0;
    proc->brk = MEM_INVALID_PAGES << PAGESHIFT;
    proc->status_queue = SpawnQueue();
    proc->pid = getPid();
    proc->parent = NULL;
    proc->user_stack_vpn_low = (VMEM_0_LIMIT >> PAGESHIFT) - KERNEL_STACK_PAGES - 2;
    proc->dead = 0;
    proc->children = SpawnQueue();
    TracePrintf(MY_TRACE_LEVEL, "createPcb END, pid is %u\n", proc->pid);
    return proc;
}

void destroyPcb(PCB *p) {
    TracePrintf(MY_TRACE_LEVEL, "destroyPCB called\n");
    if (p->ctx) {
        free(p->ctx);
    }

    DestroyQueue(p->status_queue);
    TracePrintf(MODERATE_TRACE_LEVEL, "destroyPCB - status queue was successfully destroyed\n");
    
    struct pte *pt_r0_ref = reference_pt_r0(p->pt_r0);

    for (int i = 0; i < PAGE_TABLE_LEN; i++) {
        if (pt_r0_ref[i].valid) {
            pt_r0_ref[i].valid = 0;
            addFullFreeFrame(pt_r0_ref[i].pfn);
        }
    }

    addHalfFreeFrame(p->pt_r0 / (PAGESIZE / 2));
    dereference_pt_r0(pt_r0_ref);

    free(p);
    p = NULL;
}

PCB *getNextReady() {
    TracePrintf(HIGH_TRACE_LEVEL, "GET NEXT READY\n");
    PCB *next = PollQueue(readyQueue);

    if (next == idle_proc_pcb) {
        TracePrintf(HIGH_TRACE_LEVEL, " - next ready is idle\n");
        addReady(next);
        next = PollQueue(readyQueue);
    }

    if (!next) {
        next = idle_proc_pcb;
        TracePrintf(HIGH_TRACE_LEVEL, " - next was null so i gave idle\n");
    }

    TracePrintf(HIGH_TRACE_LEVEL, " - next ready is %u\n", next->pid);
    return next;
}

void addReady(PCB *p) {
    if (!p) {
        TracePrintf(MY_TRACE_LEVEL, "addReady: NULL PCB, not adding\n");
        return;
    }
    TracePrintf(MY_TRACE_LEVEL + 1, "ADD TO READY %u\n", p->pid);
    p->clock_ticks = 0;
    PushQueue(readyQueue, p);
}

int referenced = 0;

void *reference_pt_r0(uintptr_t loc) {
    TracePrintf(8, "REFERENCE PT R0 START loc %lu\n", loc);
    int i = PAGE_TABLE_LEN - 1;

    while (pt_r1[i].valid) {
        i--;
    }
    
    pt_r1[i].valid = 1;
    pt_r1[i].pfn = (unsigned int) (loc >> PAGESHIFT);
    
    pt_r1[i].kprot = PROT_ALL;
    pt_r1[i].uprot = PROT_ALL;

    uintptr_t offset;
    if (loc % PAGESIZE == 0) {
        TracePrintf(0, "NO OFFSET\n");
        offset = 0;
    } else if ((loc - (PAGESIZE / 2)) % PAGESIZE == 0) {
        TracePrintf(0, "OFFSET EXIST\n");
        offset = PAGESIZE >> 1;
    } else {
        TracePrintf(0, "ERROR, HALF PAGE OFFSET WRONG\n");
        KernelExit(1);
    }

    TracePrintf(8, "i is %d, and pfn is %lu\n", i, pt_r1[i].pfn * 2 + (offset / (PAGESIZE >> 1)));

    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
    TracePrintf(8, "REFERENCE PT R0 END \n");
    return ((void *) ((i << PAGESHIFT) + offset + VMEM_1_BASE));
}

void dereference_pt_r0(void *spoof) {
    int idx = (int)(((uintptr_t)spoof - VMEM_1_BASE) >> PAGESHIFT);
    TracePrintf(8, "dereference pt r0, spoof, idx is %d\n", idx);
    pt_r1[idx].valid = 0;
    pt_r1[idx].pfn = 0;
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
}

