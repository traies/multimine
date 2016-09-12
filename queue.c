#include <queue.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct Q_node
{
     void * tile;
     struct Q_node * next;
} Q_node;

typedef struct Queue
{
  Q_node * first;
  Q_node * last;
} Queue;

Queue_p new_queue(void)
{
  return calloc(1, sizeof(Queue));
}

int64_t enque(Queue_p q, void * t)
{
     Q_node * node = malloc(sizeof(Q_node));
     if (!q || !t){
	  return -1;
     }
     
     if (!node) {
	  return -1;
     }
     node->next = NULL;
     node->tile = t;
     if (!q->first) {
	  q->first = node;
	  q->last = node;
     }
     else{
	  q->last->next = node;
	  q->last = node;
     }
     return 0;
}

void * deque(Queue_p q)
{
     Q_node * node;
     void * t;
     
     if (!q || !q->first){
	  return NULL;
     }
     node = q->first;
     if (node == q->last) {
	  q->first = NULL;
	  q->last = NULL;
     }
     else {
	  q->first = q->first->next;
     }
     t = node->tile;
     free(node);
     return t;
}

int8_t peek(Queue_p q)
{
     return q->first != NULL;
}

