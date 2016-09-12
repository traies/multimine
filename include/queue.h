#ifndef QUEUE_H
#define QUEUE_H

#include <stdint.h>
typedef struct Queue * Queue_p;
int64_t enque(Queue_p q, void * t);
void * deque(Queue_p q);
int8_t peek(Queue_p q);
Queue_p new_queue();

#endif
