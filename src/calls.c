#include "kernel.h"
#include "pcbManager.h"
#include "contextSwitch.h"
#include <string.h>
#include "loadProgram.h"
#include "calls.h"
#include "terminals.h"
#include "traps.h"

void PrintPageTable(uintptr_t pt_physical_address) {
    struct pte *page_table = (struct pte *)reference_pt_r0(pt_physical_address);

    TracePrintf(0, "Page Table at Physical Address: %p\n", (void *)pt_physical_address);
    TracePrintf(0, "Idx\tValid\tPFN\n");
    TracePrintf(0, "---------------------\n");

    for (int i = 0; i < PAGE_TABLE_LEN; i++) {
        TracePrintf(0, "%d\t%d\t%u\n", i, page_table[i].valid, page_table[i].pfn);
    }

    dereference_pt_r0(page_table);
}

unsigned int KernelGetPid(void) {
    TracePrintf(MY_TRACE_LEVEL + 1, "KernelGetPid START - pid %u\n\n\n", current_process->pid);
    return current_process->pid;
}

int KernelFork(void) {

    uintptr_t i;

    TracePrintf(MY_TRACE_LEVEL + 1, "\n\nKernelFork START - pid %u\n\n\n", current_process->pid);
    PCB *child = createPcb();
    if (!child) return ERROR;

    child->parent = current_process;
    current_process->num_children++;

    struct pte *parent_pt_r0 = reference_pt_r0(current_process->pt_r0);
    struct pte *child_pt_r0 = reference_pt_r0(child->pt_r0);

    int needed_pages = 0;
    for (int i = 0; i < PAGE_TABLE_LEN; ++i) {
        if (parent_pt_r0[i].valid &&
            !(i >= (KERNEL_STACK_BASE >> PAGESHIFT))) {
            needed_pages++;
        }
    }

    if (needed_pages > ffpa_put_loc) {
        TracePrintf(0, "KernelFork: not enough free frames\n");
        destroyPcb(child);
        dereference_pt_r0(parent_pt_r0);
        dereference_pt_r0(child_pt_r0);
        return ERROR;
    }

    TracePrintf(MY_TRACE_LEVEL + 1, "KernelFork brk is %lu\n", current_process->brk);
    
    int j = PAGE_TABLE_LEN - 1;
    while (pt_r1[j].valid) {
        j--;
    }

    int tmp = j;
    
    pt_r1[j].valid = 1;
    pt_r1[j].kprot = PROT_ALL;
    pt_r1[j].uprot = PROT_ALL;
    
    // will copy pages
    TracePrintf(MY_TRACE_LEVEL + 1, "KernelFork Copy parent pages\n");
    
    for (i = 0; i < PAGE_TABLE_LEN - KERNEL_STACK_PAGES; i++) {
        if (parent_pt_r0[i].valid) {
            // TracePrintf(MY_TRACE_LEVEL + 1, "KernelFork i is %lu\n", i);
            unsigned int frame = getFullFreeFrame();
            if (frame == (unsigned int) -1) {
                TracePrintf(0, "No free frames available\n");
                KernelExit(1);
            }

            pt_r1[tmp].pfn = frame;
            WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
            memcpy((void *)((uintptr_t)(tmp + PAGE_TABLE_LEN) << PAGESHIFT), (void *)(i << PAGESHIFT), PAGESIZE);
            
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

    // no need flush register, as this is done in derefernce_pt_r0

    TracePrintf(MY_TRACE_LEVEL + 1, "KernelFork end copying \n");
    
    child->brk = current_process->brk;
    child->user_stack_vpn_low = current_process->user_stack_vpn_low;

    PushQueue(current_process->children, child);
    num_procs++;

    // PrintPageTable((uintptr_t)child->pt_r0);
    // PrintPageTable((uintptr_t)current_process->pt_r0);

    TracePrintf(MY_TRACE_LEVEL + 1, "PRE CTX SWITCH IN FORK \n");
    int rc = ContextSwitch(MySwitch, child->ctx, current_process, child);
    
    TracePrintf(MY_TRACE_LEVEL + 1, "POST CTX SWITCH IN FORK \n");
    if (rc == -1) {
        return ERROR;
    }

    if (current_process->pid == child->pid) {
        TracePrintf(MY_TRACE_LEVEL + 1, "KernelFork END for CHILD\n");
        return 0;
    } else {
        TracePrintf(MY_TRACE_LEVEL + 1, "KernelFork END for PARENT\n");
        addReady(child);
        return child->pid;
    }
}

int KernelBrk(void *addr) {
    TracePrintf(MY_TRACE_LEVEL + 1, "\n\nKernelBrk START - pid %u\n\n\n", current_process->pid);
    TracePrintf(MY_TRACE_LEVEL + 1, "KernelBrk WANT addr is %p\n", addr);
    TracePrintf(MY_TRACE_LEVEL + 1, "KernelBrk brk is %p\n", (void*) current_process->brk);

    if (addr == NULL || (uintptr_t) addr < MEM_INVALID_SIZE || (int) (((uintptr_t) addr >> PAGESHIFT) + 1) >= current_process->user_stack_vpn_low) {
        // If any error is encountered (for example, if not enough memory is available or if the address addr is invalid), the value ERROR is returned.
        //KernelExit(ERROR);
        return ERROR;
    }

    struct pte *pt_r0_ref = reference_pt_r0(current_process->pt_r0);
    uintptr_t old_brk = current_process->brk;
    uintptr_t new_brk = (uintptr_t) addr;

    uintptr_t old_pg = UP_TO_PAGE(old_brk);
    uintptr_t new_pg = UP_TO_PAGE(new_brk);

    if (new_pg > old_pg) {
        TracePrintf(4, "KernelBrk: Allocating pages\n");
        for (uintptr_t va = old_pg; va < new_pg; va += PAGESIZE) {
            int vpn = va >> PAGESHIFT;
            if (pt_r0_ref[vpn].valid) continue;

            int pfn = getFullFreeFrame();
            if (pfn == -1) return ERROR;

            pt_r0_ref[vpn].valid = 1;
            pt_r0_ref[vpn].pfn = pfn;
            pt_r0_ref[vpn].kprot = PROT_READ | PROT_WRITE;
            pt_r0_ref[vpn].uprot = PROT_READ | PROT_WRITE;
        }
    } else if (new_pg < old_pg) {
        TracePrintf(4, "KernelBrk: Deallocating pages\n");
        for (uintptr_t va = new_pg; va < old_pg; va += PAGESIZE) {
            int vpn = va >> PAGESHIFT;
            if (!pt_r0_ref[vpn].valid) continue;
            addFullFreeFrame(pt_r0_ref[vpn].pfn);
            pt_r0_ref[vpn].valid = 0;
            pt_r0_ref[vpn].pfn = 0;
            pt_r0_ref[vpn].kprot = PROT_NONE;
            pt_r0_ref[vpn].uprot = PROT_NONE;

        }
    }
    current_process->brk = new_brk;
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
    dereference_pt_r0(pt_r0_ref);
    TracePrintf(MY_TRACE_LEVEL + 1, "\n\n\nKernelBrk END SUCCESS\n\n");
    return 0;
}


int KernelExec(char *filename, char **argvec)
{
    TracePrintf(MY_TRACE_LEVEL + 1, "\n\nKernelExec START - pid %u\n\n\n", current_process->pid);
    int success;

    if (filename == NULL || argvec == NULL) return ERROR;

    if (VerifyInputString(filename) != 0 || VerifyArrayOfPointers(argvec) != 0) {
        TracePrintf(MY_TRACE_LEVEL + 1, "KernelExec - filename OR argvec failed verification\n");
        return ERROR;
    }
   
    success = LoadProgram(current_exception, filename, argvec);

    // Current process can still run. Return ERROR to calling program.
    if (success == -1) {
        return ERROR;
    } 
    // Current process is no longer runnable. Terminate and run another process. 
    if (success == -2) {
        KernelExit(ERROR);
    }
    TracePrintf(MY_TRACE_LEVEL + 1, "\n\n\nKernelExec END SUCCESS\n\n");
    
    return 0;

}

/* The calling process is blocked until clock_ticks clock interrupts have occurred after the call.
Upon completion of the delay, the value 0 is returned. If clock_ticks is 0, return is immediate.
If clock_ticks is less than 0, ERROR is returned instead. */
int 
KernelDelay(int clock_ticks) 
{   
    TracePrintf(MY_TRACE_LEVEL + 1, "\n\nKernelDelay START - pid %u\n\n\n", current_process->pid);
    if (clock_ticks < 0) return ERROR;
    if (clock_ticks == 0) return 0;
    
    current_process->clock_ticks = clock_ticks;
    TracePrintf(0, "ENQUE BLOCKED P: %p\n", current_process);
    PushQueue(blockedQueue, current_process);

    PCB *next_proc = getNextReady();

    if (next_proc == NULL) {
        next_proc = idle_proc_pcb;
    }

    ContextSwitch(MySwitch, current_process->ctx, current_process, next_proc);
    TracePrintf(MY_TRACE_LEVEL + 1, "\n\n\nKernelDelay END SUCCESS\n\n");
    return 0;
}

unsigned int
KernelWait(int *status_ptr)
{
    TracePrintf(MY_TRACE_LEVEL + 1, "\n\nKernelWait START - pid %u\n\n\n", current_process->pid);
    
    // If num_children > 0 and there are no exited child processes, block the process (context switch and enqueue at kernelWait_queue) until the next child is
    // is termininated by the kernel (if process is terminated by the kernel. Its exit status should appear to its parent as ERROR.
    
    SQ_DATA *statusInfo;
    if ((statusInfo = PollQueue(current_process->status_queue))) {
        unsigned int pid = statusInfo->pid;
        *status_ptr = statusInfo->exit_status;
        free(statusInfo);
        current_process->num_children--;
        return pid;
    }
    
    PCB *next_proc = getNextReady();

    PushQueue(waitQueue, current_process);
    ContextSwitch(MySwitch, current_process->ctx, current_process, next_proc);

    // re: logic
    if ((statusInfo = PollQueue(current_process->status_queue))) {
        int pid = statusInfo->pid;
        *status_ptr = statusInfo->exit_status;
        free(statusInfo);
        TracePrintf(MY_TRACE_LEVEL + 1, "\n\n\nKernelWait END SUCCESS\n\n");
        current_process->num_children--;
        return pid;
    }

    return ERROR;
} 

void KernelExit(int status) {
    TracePrintf(MY_TRACE_LEVEL + 1, "\n\nKernelExit START - pid %u\n\n\n", current_process->pid);
    PCB *exiting = current_process;

    if (exiting->parent) {
        TracePrintf(MY_TRACE_LEVEL + 1, "KernelExit: parent is %u\n", exiting->parent->pid);
        SQ_DATA *status_entry = malloc(sizeof(SQ_DATA));
        status_entry->pid = exiting->pid;
        status_entry->exit_status = status;
        PushQueue(exiting->parent->status_queue, status_entry);
        RemoveFromQueue(waitQueue, exiting->parent);
        addReady(exiting->parent);
    }

    PCB *next_proc = getNextReady();

    current_process->dead = 1;
    TracePrintf(MY_TRACE_LEVEL + 1, "\n\n\nKernelExit END SUCCESS stat code %d\n\n", status);
    num_procs--;

    printf("process pid %d exited with status %d\n", exiting->pid, status);
    ContextSwitch(MySwitch, current_process->ctx, current_process, next_proc);
    
}


int 
KernelTTYRead(int tty_id, void *buf, int len)
{
    if (VerifyInputString(buf) != 0) {
        TracePrintf(MY_TRACE_LEVEL + 1, "KernelTTYWrite - buf failed verification\n");
        return ERROR;
    }
    TracePrintf(MY_TRACE_LEVEL, "\n\nKernelTTYRead START - pid %u\n\n\n", current_process->pid);
    // Validation.
    if (len < 0 || buf == NULL) {
        return ERROR;
    }

    Terminal *tty = &terminals[tty_id];

    LINE_DATA *line_data = PollQueue(tty->recieved_lines);

    PCB *cur_blocked = PollQueue(tty->BlockedReadersQueue);
    if (cur_blocked) {
        TracePrintf(0, " : there is a line for readers\n");
        PushQueue(tty->BlockedReadersQueue, current_process);
        addReady(cur_blocked);
        PCB *next = getNextReady();
        ContextSwitch(MySwitch, current_process->ctx, current_process, next);
    }

    // now we are meant to be the next reader
    while (!line_data) {
        // add proc to reader blocked
        TracePrintf(0, " : there is no avails string\n");
        PushQueue(tty->BlockedReadersQueue, current_process);
        PCB *next = getNextReady();
        ContextSwitch(MySwitch, current_process->ctx, current_process, next);
        
        line_data = PollQueue(tty->recieved_lines);
    }

    // now there is a string and we want it

    char *str = line_data->str;
    int offset = line_data->offset;
    int len_to_copy = line_data->length - offset;

    if (len < len_to_copy) {
        TracePrintf(MY_TRACE_LEVEL, "KernelTTYRead: CASE 1 nis %d\n", len);
        // we need to copy only part of the string
        memcpy(buf, &str[offset], len);
        line_data->offset += len;
        PushQueueFront(tty->recieved_lines, line_data);
    } else {
        TracePrintf(MY_TRACE_LEVEL, "KernelTTYRead: CASE 2 nis %d\n", len_to_copy);
        // we need to copy the whole remainder of string
        memcpy(buf, &str[offset], len_to_copy);
        free(line_data);
    }

    return len_to_copy;
 }

int 
KernelTTYWrite(int tty_id, void *buf, int len)
{   
    if (VerifyInputString(buf) != 0) {
        TracePrintf(MY_TRACE_LEVEL + 1, "KernelTTYWrite - buf failed verification\n");
        return ERROR;
    }
    TracePrintf(MY_TRACE_LEVEL, "\n\nKernelTTYWrite START - pid %u\n\n\n", current_process->pid);
    PCB *next_proc;

    if (len < 0 || len > TERMINAL_MAX_LINE || buf == NULL) {
        return ERROR;
    }

    if (len == 0) {
        return 0;
    }

    // If terminal at tty_id is currently writing, add current process to PendingOutputsQueue() and context switch.
    while (terminals[tty_id].outputPCB !=  NULL){
        TracePrintf(MY_TRACE_LEVEL, "KernelTTYWrite: tty_id %d is busy writing\n", tty_id);

        PushQueue(terminals[tty_id].PendingOutputsQueue, current_process);

        next_proc = (PCB *) getNextReady();

        ContextSwitch(MySwitch, current_process->ctx, current_process, next_proc);
    }


    /* Terminal tty_id is available to write */


    // Set up the current writer
    strcpy(terminals[tty_id].write_buf, buf);
    terminals[tty_id].outputPCB = current_process;

    // Begin the TtyTransmit.
    TtyTransmit(tty_id, terminals[tty_id].write_buf, len);
   
    return len;
}

int VerifyInputString(char *str) {
    if (str == NULL) return ERROR;

    uintptr_t addr = (unsigned long) str;
    struct pte *pt_r0 = reference_pt_r0(current_process->pt_r0);

    while (1) {
        int vpn = addr >> PAGESHIFT;
        // || vpn >= VMEM_1_LIMIT - KERNEL_STACK_PAGES
        // Out of user bounds  - region 1
        if (vpn > VMEM_0_LIMIT - KERNEL_STACK_BASE || vpn < MEM_INVALID_PAGES) {
            TracePrintf(MY_TRACE_LEVEL + 1, "VerifyInputStrin - lower vpn than stack = %d, low stack vpn = %d\n", vpn, VMEM_0_LIMIT - KERNEL_STACK_BASE);
            dereference_pt_r0(pt_r0);
            return ERROR;
        }

        // Page not valid or not (readable and writeable)
        if (!pt_r0[vpn].valid || !(pt_r0[vpn].uprot & PROT_READ & PROT_WRITE)) {
            TracePrintf(MY_TRACE_LEVEL + 1, "VerifyInputStrig - page not valid or not readable. VPN =%d\n", vpn);
            dereference_pt_r0(pt_r0);
            return ERROR;
        }

        // Get how many bytes are left in this page
        int offset = addr % PAGESIZE;
        int bytes_left = PAGESIZE - offset;

        // Scan this page for the null byte
        for (int i = 0; i < bytes_left; i++) {
            char c = *((char *)(addr + i));
            TracePrintf(MY_TRACE_LEVEL + 1, "VerifyInputString - char = %c\n", c);
            if (c == '\0') {
                dereference_pt_r0(pt_r0);
                return 0;  // Success, string is null-terminated
            }
        }

        // Move to the next page
        addr = DOWN_TO_PAGE(addr + PAGESIZE);
    }
    // Should not reach here
    TracePrintf(MY_TRACE_LEVEL + 1, "VerifyInputStrig - SHOULD NOT REACH HERE\n");
    dereference_pt_r0(pt_r0);
    return ERROR;  
}


int VerifyArrayOfPointers(char **argvec) {
    if (argvec == NULL) return ERROR;

    // Start walking the array of pointers
    for (int i = 0;; i++) {

        // Stop when we reach the terminating NULL
        if (argvec[i] == NULL) {
            break;
        }

        // Check the string that argvec[i] points to
        if (VerifyInputString(argvec[i]) == ERROR) {
            return ERROR;
        }
    }

    return 0;  // All good
}