#include "queue.h"
#include "kernel.h"
#include "pcbManager.h"

void PushQueueFront(Queue *q, void *d) {
    // sanity check
    if (!q) {
        TracePrintf(0, "PushQueueFront: NULL queue, not adding\n");
        return;
    }

    if (!d) {
        TracePrintf(0, "PushQueueFront: NULL data, not adding\n");
        return;
    }

    // queue op
    QueueNode *node = malloc(sizeof(QueueNode));
    node->data = d;
    node->next = q->head;

    if (q->head == NULL) {
        q->tail = node;
    }
    
    q->head = node;
}


void PushQueue(Queue *q, void *d) {
    // sanity check
    if (!q) {
        TracePrintf(0, "PushQueue: NULL queue, not adding\n");
        return;
    }

    if (!d) {
        TracePrintf(0, "PushQueue: NULL data, not adding\n");
        return;
    }

    // queue op
    QueueNode *node = malloc(sizeof(QueueNode));
    node->data = d;
    node->next = NULL;
    
    if (q->head == NULL) {
        q->head = node;
        q->tail = node;
        return;
    }
    
    q->tail->next = node;
    q->tail = q->tail->next;
    
    return;
}

void *PollQueue(Queue *q) {
    if (!q || !q->head) {
        return NULL;
    }

    QueueNode *old_head = q->head;

    if (!old_head) {
        return NULL;
    }
    
    void *ret = old_head->data;

    q->head = old_head->next;

    if (q->head == NULL) q->tail = NULL;

    free(old_head);
    return ret;
}

/* Remove the node containing data from q, does not free data*/
void RemoveFromQueue(Queue *q, void *d) {
    if (!q) {
        return;
    }

    if (q->head == NULL) {
        return;
    }
    
    if (q->head->data == d) {
        QueueNode *tmp = q->head;
        q->head = q->head->next;
        free(tmp);
        return;
    }
    
    QueueNode *prev = q->head;
    QueueNode *qn = q->head->next;

    while (qn != NULL) {
        if (qn->data == d) {
            prev->next = qn->next;
            free(qn);
            return;
        }
        prev = qn;
        qn = qn->next;
    }
}

Queue *SpawnQueue() {
    Queue *q = malloc(sizeof(Queue));
    q->head = NULL;
    q->tail = NULL;

    return q;
}

/* destroys the queue and all data inside it */
void DestroyQueue(Queue *q) {
    TracePrintf(MY_TRACE_LEVEL, "DESTROYQUEUE START\n");
    if (!q) return;

    QueueNode *it = q->head;

    TracePrintf(MY_TRACE_LEVEL, " : HEAD GOT ADDR %p\n", it);

    while (it) {
        QueueNode *next = it->next;
        TracePrintf(MY_TRACE_LEVEL, " : ABOUT TO FREE DATA\n");
        free(it->data);
        TracePrintf(MY_TRACE_LEVEL, " : ABOUT TO FREE THIS NODE\n");
        free(it);
        it = next;
    }

    TracePrintf(MY_TRACE_LEVEL, " : ABOUT OT FREE QUEUE\n");
    free(q);
    TracePrintf(MY_TRACE_LEVEL, "DESTROYQUEUE END\n");
}



void PrintQueue(Queue *q, char *name) {
    TracePrintf(0, "------- START PRINT %s QUEUE --------------\n", name);
    if (!q || !q->head) {
        TracePrintf(0, "Queue is empty.\n");
        TracePrintf(0, "------- END PRINT %s QUEUE --------------\n", name);
        return;
    }

    TracePrintf(0, "Queue contents (PIDs):\n");

    QueueNode *current = q->head;

    TracePrintf(0, "HEAD: %p   %u\n", current, ((PCB *)current->data)->pid);
    TracePrintf(0, "TAIL: %p   %u\n", q->tail, ((PCB *)q->tail->data)->pid);

    while (current) {
        if (!current->data) {
            TracePrintf(0, "NULL node data!\n");
            current = current->next;
            continue;
        }
        
        PCB *pcb = (PCB *)current->data; // Cast data to PCB pointer
        TracePrintf(0, "PID: %d\n", pcb->pid);
        current = current->next;
        if (current == q->tail && current->next == q->head) {
            TracePrintf(0, "QUEUE IS CIRCULAR\n");
            break;
        }
    }
    TracePrintf(0, "------- END PRINT %s QUEUE --------------\n", name);
}