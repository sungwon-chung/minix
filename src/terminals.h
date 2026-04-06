#ifndef _TERMINALS_H
#define _TERMINALS_H

#include <hwsim/hardware.h>
#include "queue.h"
#include "pcbManager.h"

typedef struct line_data {
    char str[TERMINAL_MAX_LINE];
    int length;   // # chars received from terminal
    int offset;
} LINE_DATA;

typedef struct terminal
{
    char write_buf[TERMINAL_MAX_LINE]; // Buffer for the terminal
    Queue *PendingOutputsQueue;        // Terminals will become blocked at writing
    Queue *BlockedReadersQueue;        // Queue of processes waiting to read from the terminal
    Queue *recieved_lines;          // Queue of lines that have been received
    PCB *outputPCB;                     // Process currently in control of the terminal for output.
} Terminal;

#endif