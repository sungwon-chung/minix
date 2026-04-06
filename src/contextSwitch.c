#include "kernel.h"
#include<string.h>

void copyKernelStack(PCB *from, PCB *to) {

    struct pte *parent_pt_r0 = reference_pt_r0(from->pt_r0);
    struct pte *child_pt_r0 = reference_pt_r0(to->pt_r0);

    int j = PAGE_TABLE_LEN - 1;
    while (pt_r1[j].valid) {
        j--;
    }

    int tmp = j;
    
    pt_r1[j].valid = 1;
    pt_r1[j].kprot = PROT_ALL;
    pt_r1[j].uprot = PROT_ALL;
        
    for (int i = (int) (KERNEL_STACK_BASE >> PAGESHIFT); i < (int) (KERNEL_STACK_LIMIT >> PAGESHIFT); i++) {
        if (parent_pt_r0[i].valid) {
            unsigned int frame = getFullFreeFrame();
            if (frame == (unsigned int) -1) {
                TracePrintf(0, "No free frames available\n");
                KernelExit(1);
            }
            pt_r1[tmp].valid = 1;
            pt_r1[tmp].pfn = frame;
            WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
            memcpy((void *)((uintptr_t)(tmp + PAGE_TABLE_LEN) << PAGESHIFT), (void *) ((uintptr_t) i << PAGESHIFT), PAGESIZE);
            pt_r1[tmp].valid = 0;
            
            child_pt_r0[i].valid = parent_pt_r0[i].valid;
            child_pt_r0[i].pfn = pt_r1[tmp].pfn;
            child_pt_r0[i].kprot = parent_pt_r0[i].kprot;
            child_pt_r0[i].uprot = parent_pt_r0[i].uprot;

            if (child_pt_r0[i].pfn >= (VMEM_1_LIMIT >> PAGESHIFT)) {
                TracePrintf(0, "BAD PFN at i=%lu: %u\n", i, child_pt_r0[i].pfn);
                KernelExit(1);
            }
        } else {
            child_pt_r0[i].valid = 0;
            child_pt_r0[i].kprot = PROT_NONE;
            child_pt_r0[i].uprot = PROT_NONE;
            child_pt_r0[i].pfn = 0;
        }
    }

    // cleanup
    pt_r1[tmp].valid = 0;
    dereference_pt_r0(parent_pt_r0);
    dereference_pt_r0(child_pt_r0);

}

SavedContext *MySwitch(SavedContext *ctxp, void *p1, void *p2) {
    TracePrintf(MY_TRACE_LEVEL, "MySwitch: start\n");

    PCB *from = (PCB *) p1;
    PCB *to = (PCB *) p2;

    if (haltable && num_procs == 0) {
        TracePrintf(0, "NO MORE PROCS, HALTING NOW\n");
        Halt();
    }

    if (!from) {
        TracePrintf(MY_TRACE_LEVEL, "MySwitch: case 1\n");
        WriteRegister(REG_PTR0, (RegVal) to->pt_r0);
        WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
        current_process = to;
        TracePrintf(MY_TRACE_LEVEL, "MySwitch: end | new pid : %u\n", current_process->pid);
        return to->ctx;
    }

    if (ctxp == to->ctx) {
        if (from == to) {
            TracePrintf(MY_TRACE_LEVEL, "MySwitch: case 2 - DO NOTHING\n");
            return to->ctx; // no switch
        }

        copyKernelStack(from, to);
        TracePrintf(MY_TRACE_LEVEL, "MySwitch: case 2\n");
        current_process = from;
        TracePrintf(MY_TRACE_LEVEL, "MySwitch: end | new pid : %u\n", current_process->pid);
        return to->ctx;
    }
    TracePrintf(MY_TRACE_LEVEL, "MySwitch: case 3\n");

    if (current_process->dead) {
        QueueNode *iter = current_process->children->head;
        while (iter) {
            QueueNode *next = iter->next;
            PCB *child = (PCB *) iter->data;
            child->parent = NULL;
            free(iter);
            iter = next;
        }
        free(current_process->children);
        destroyPcb(current_process);
    }

    current_process = to;
    WriteRegister(REG_PTR0, (RegVal) to->pt_r0);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
    
    TracePrintf(MY_TRACE_LEVEL, "MySwitch: end | new pid : %u\n", current_process->pid);
    return to->ctx;
}