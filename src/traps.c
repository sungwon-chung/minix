#include "kernel.h"
#include "pcbManager.h"
#include "contextSwitch.h"
#include "calls.h"
#include <stdio.h>

ExceptionInfo *current_exception;

void TrapKernelHandler(ExceptionInfo *info) {
    TracePrintf(MY_TRACE_LEVEL, "TrapKernelHandler called\n");
    current_exception = info;
    switch(info->code) {
        case minix_FORK:
            info->regs[0] = KernelFork();
            break;
        case minix_EXEC:
            info->regs[0] = KernelExec((char*)(info->regs[1]), (char(**))(info->regs[2]));
            break;
        case minix_EXIT:
            KernelExit((int) info->regs[1]);
            break;
        case minix_WAIT:
            info->regs[0] = KernelWait((int*) info->regs[1]);
            break;
        case minix_GETPID:
            info->regs[0] = KernelGetPid();
            break;
        case minix_BRK:
            info->regs[0] = KernelBrk((void*) info->regs[1]);
            break;
        case minix_DELAY:
            info->regs[0] = KernelDelay((int) info->regs[1]);
            break;
        case minix_TTY_READ:
            info->regs[0] = KernelTTYRead((int) info->regs[1], (void*) info->regs[2], (int) info->regs[3]);
            break;
        case minix_TTY_WRITE:
            info->regs[0] = KernelTTYWrite((int) info->regs[1], (void*) info->regs[2], (int) info->regs[3]);
            break;
        default:
            break;

    }
}
void TrapClockHandler(ExceptionInfo *info) {
    TracePrintf(MY_TRACE_LEVEL + 1, "TrapClockHandler called\n");

    PrintQueue(readyQueue, "READY");
    PrintQueue(blockedQueue, "BLOCKED");
    PrintQueue(waitQueue, "WAIT");
    current_exception = info;
    
    Queue *newBlocked = SpawnQueue();
    PCB *p;
    while ((p = PollQueue(blockedQueue))) {
        TracePrintf(MY_TRACE_LEVEL, "TrapClockHandler - DEQUE P: %p\n",p);
        
        if (--(p->clock_ticks) <= 0) {
            addReady(p);
        } else {
            PushQueue(newBlocked, p);
        }
    }

    DestroyQueue(blockedQueue);
    blockedQueue = newBlocked;

    // now cur proc
    current_process->clock_ticks++;
    if (current_process->clock_ticks < 2) {
        TracePrintf(MODERATE_TRACE_LEVEL + 1, "TrapClockHandler - less than 2 clock ticks\n");
        return;
    }

    addReady(current_process);
    PCB *next_proc = getNextReady();
    // PrintQueue(readyQueue);
    ContextSwitch(MySwitch, current_process->ctx, current_process, next_proc);
}



void TrapMemoryHandler(ExceptionInfo *info) {

    PrintPageTable((uintptr_t) current_process->pt_r0);
    PrintPageTable((uintptr_t) pt_r1);
    TracePrintf(MY_TRACE_LEVEL, "TRAP MEM: START addr %p\n", info->addr);
    TracePrintf(MY_TRACE_LEVEL, "TRAP MEM: START CODE 0x%d\n", info->code);
    // logger(info);
    TracePrintf(0, "FAULT PC: %p, SP: %p, ADDR: %p\n", info->pc, info->sp, (uintptr_t) info->addr);
    

    uintptr_t want = (uintptr_t) info->addr;

    int want_vpn = DOWN_TO_PAGE(want) >> PAGESHIFT;
    
    TracePrintf(MY_TRACE_LEVEL,"fault vpn, %d\n", want_vpn);
    // Must fall in Region 0 and be less than current user stack and greater than user brk
    if (want >= VMEM_0_LIMIT - KERNEL_STACK_SIZE 
        || (uintptr_t) DOWN_TO_PAGE(want) <= current_process->brk + PAGESIZE
        || want >= USER_STACK_LIMIT) {
            if (info->code == TRAP_MEMORY_MAPERR) {
                TracePrintf(7,"No mapping at addr\n");
            }
            if (info->code == TRAP_MEMORY_ACCERR) {
                TracePrintf(7,"Prot violation at addr\n");
            }
            if (info->code == TRAP_MEMORY_KERNEL) {
                TracePrintf(7,"Linux kernel sent SIGSEGV at addr\n");
            }
            if (info->code == TRAP_MEMORY_USER) {
                TracePrintf(7,"Received SIGSEGV from user\n");
            }

        TracePrintf(MY_TRACE_LEVEL, "TRAP MEM: INVAL MEM ACCESS | vpn = %d\n", want_vpn);
        KernelExit(ERROR);
    }

    struct pte *pt_r0_ref = reference_pt_r0(current_process->pt_r0);

    for (; current_process->user_stack_vpn_low >= want_vpn; current_process->user_stack_vpn_low--) {
        unsigned int needed_pfn = getFullFreeFrame();
        if (needed_pfn == (unsigned int)-1) {
            KernelExit(ERROR);
            return;
        }
        pt_r0_ref[current_process->user_stack_vpn_low].valid = 1;
        pt_r0_ref[current_process->user_stack_vpn_low].pfn = needed_pfn;
        pt_r0_ref[current_process->user_stack_vpn_low].kprot = PROT_READ | PROT_WRITE;
        pt_r0_ref[current_process->user_stack_vpn_low].uprot = PROT_READ | PROT_WRITE;

        TracePrintf(MY_TRACE_LEVEL, "TRAP MEM: Mapped VPN %d to pfn %d\n", current_process->user_stack_vpn_low, needed_pfn);
        
    }

    TracePrintf(MY_TRACE_LEVEL, "TRAP MEM: SUCCESS | last want_vpn:  %d\n", want_vpn);
    dereference_pt_r0(pt_r0_ref);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
}

void TrapMathHandler(ExceptionInfo *info) {
    TracePrintf(MY_TRACE_LEVEL, "TrapMathHandler called\n");
    current_exception = info;
    // Return pid of terminated process.
    fprintf(stderr, "PID: %d | ", current_process->pid);
    switch(info->code) {
        case TRAP_MATH_INTDIV:
            fprintf(stderr, "Integer divide by zero\n");
            break;
        case TRAP_MATH_INTOVF: 
            fprintf(stderr, "Integer overflow\n");
            break;
        case TRAP_MATH_FLTDIV:
            fprintf(stderr, "Floating divide by zero\n");
            break;
        case TRAP_MATH_FLTOVF:
            fprintf(stderr, "Floating overflow\n");
            break;
        case TRAP_MATH_FLTUND:
            fprintf(stderr, "Floating underflow\n");
            break;
        case TRAP_MATH_FLTRES:
            fprintf(stderr, "Floating inexact result\n");
            break;
        case TRAP_MATH_FLTINV:
            fprintf(stderr, "Invalid floating operation\n");
            break;
        case TRAP_MATH_FLTSUB:
            fprintf(stderr, "FP subscript out of range\n");
            break;
        case TRAP_MATH_KERNEL:
            fprintf(stderr, "Linux kernel sent SIGFPE\n");
            break;
        case TRAP_MATH_USER:
            fprintf(stderr, "Received SIGFPE from user\n");
            break;
        default:
            fprintf(stderr, "\n");
            break;
    }

    // Math traps are unrecoverable - terminate currently running process. 
    KernelExit(ERROR);
}

void TrapIllegalHandler(ExceptionInfo *info)
{
    TracePrintf(MY_TRACE_LEVEL, "TrapIllegalHandler called\n");
    current_exception = info;
    // Return pid of terminated process.
    fprintf(stderr, "PID: %d | ", current_process->pid);
    switch(info->code) {
        case TRAP_ILLEGAL_ILLOPC:
            fprintf(stderr, "Illegal opcode\n");
            break;
        case TRAP_ILLEGAL_ILLOPN:
            fprintf(stderr, "Illegal operand\n");
            break;
        case TRAP_ILLEGAL_ILLADR:
            fprintf(stderr, "Illegal addressing mode\n");
            break;
        case TRAP_ILLEGAL_ILLTRP:
            fprintf(stderr, "Illegal software trap\n");
            break;
        case TRAP_ILLEGAL_PRVOPC:
            fprintf(stderr, "Privileged opcode\n");
            break;
        case TRAP_ILLEGAL_PRVREG:
            fprintf(stderr, "Privileged register\n");
            break;
        case TRAP_ILLEGAL_COPROC:
            fprintf(stderr, "Coprocessor error\n");
            break;
        case TRAP_ILLEGAL_BADSTK:
            fprintf(stderr, "Bad stack\n");
            break;
        case TRAP_ILLEGAL_KERNELI: 
            fprintf(stderr, "Linux kernel sent SIGILL\n");
            break;
        case TRAP_ILLEGAL_USERIB:
            fprintf(stderr, "Received SIGILL or SIGBUS from user\n");
            break;
        case TRAP_ILLEGAL_ADRALN:
            fprintf(stderr, "Invalid address alignment\n");
            break;
        case TRAP_ILLEGAL_ADRERR:
            fprintf(stderr, "Non-existent physical address\n");
            break;
        case TRAP_ILLEGAL_OBJERR:
            fprintf(stderr, "Object-specific HW error\n");
            break;
        case TRAP_ILLEGAL_KERNELB:
            fprintf(stderr, "Linux kernel sent SIGBUS\n");
            break;
        default:
            fprintf(stderr, "\n");
            break;
    }

    // Illegal traps are unrecoverable - terminate currently running process. 
    KernelExit(ERROR);
}

void
TrapTTYReceiveHandler(ExceptionInfo *info)
{
    TracePrintf(MY_TRACE_LEVEL, "TrapTTYReceiveHandler called\n");

    int tty_id = info->code;
    Terminal *tty = &terminals[tty_id];
    LINE_DATA *line_data = malloc(sizeof(LINE_DATA));
    // Currently no offset;
    line_data->offset = 0;
    // Set the number of read characters
    line_data->length = 0;

    
    // Get a full line of input from this terminal.
    int numC_received = TtyReceive(tty_id, line_data->str, TERMINAL_MAX_LINE);
    
    // Set the number of read characters
    

    if (line_data->str[numC_received - 1] == '\n') {
        line_data->length = numC_received - 1;
    } else {
        line_data->length = numC_received;
    }

    

    
    // Push updated line_data to received_lines queue. 
    PushQueue(tty->recieved_lines, line_data);

    PCB *next_proc = PollQueue(terminals[tty_id].BlockedReadersQueue);

    if (!next_proc) {
        TracePrintf(MY_TRACE_LEVEL, " : no blocked in this term\n");
        next_proc = getNextReady();
    }

    if (current_process != next_proc) {
        addReady(current_process);
    }
    

    TracePrintf(MY_TRACE_LEVEL, "TrapTTYReceiveHandler: %d chars received\n", numC_received);

    ContextSwitch(MySwitch, current_process->ctx, current_process, next_proc);
    
}

void
TrapTTYTransmitHandler(ExceptionInfo *info)
{
    TracePrintf(MY_TRACE_LEVEL, "TrapTTYTransmitHandler called\n");
    int tty_id =info->code;
    
    PCB *next_proc = PollQueue(terminals[tty_id].PendingOutputsQueue);

    if (!next_proc) {
        TracePrintf(MY_TRACE_LEVEL, " : no blocked in this term\n");
        next_proc = getNextReady();
    }

    addReady(current_process);
    if (current_process != terminals[tty_id].outputPCB) {
        addReady(terminals[tty_id].outputPCB);
    }
    terminals[tty_id].outputPCB = NULL;

    TracePrintf(MY_TRACE_LEVEL, "TrapTTYReceiveHandler: %d chars received\n");

    ContextSwitch(MySwitch, current_process->ctx, current_process, next_proc);
}
