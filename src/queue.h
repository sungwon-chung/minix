#ifndef _QUEUE_H
#define _QUEUE_H

typedef struct queueNode {
    void *data;
    struct queueNode *next;
} QueueNode;

typedef struct queue {
    QueueNode *head;
    QueueNode *tail;
} Queue;

extern void PushQueue(Queue *q, void *d);
extern void PushQueueFront(Queue *q, void *d);
extern void* PollQueue(Queue *q);
extern void RemoveFromQueue(Queue *q, void *d);
extern Queue *SpawnQueue();
extern void DestroyQueue(Queue *q);

extern void PrintQueue(Queue *q, char *name);
#endif