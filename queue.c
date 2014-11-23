#include "init.h"

QUEUE* createQueue(void){
	QUEUE* queue;

	queue = (QUEUE *)malloc(sizeof(QUEUE));
    if (queue)
    {
        queue->front = NULL;
        queue->rear = NULL;
        queue->count = 0;
    }
    return queue;
}

void enqueue(QUEUE* queue, pcb* proc_id){
	node *newPtr;
 
    if (!(newPtr = (node*)malloc(sizeof(node)))) return;

    newPtr->proc = proc_id;
    newPtr->next = NULL;
	newPtr->prev = queue->rear;

    if (queue->count == 0) queue->front = newPtr;
    else queue->rear->next = newPtr;

    (queue->count)++;
    queue->rear = newPtr;
    return;
}

node* dequeue(QUEUE* queue){
	node *temp;

    if (!queue->count) return 0;

    temp = queue->front;
    if (queue->count == 1) queue->rear = queue->front = NULL;
    else queue->front = queue->front->next;

    (queue->count)--;
    return temp;
}

void printQueue(QUEUE* queue){
	node *temp;

	printf("<");
	if(queue->front != NULL){
    	for(temp = queue->front; temp->next != NULL; temp = temp->next){
       		printf("%d, ", temp->proc->od);
    	}
		printf("%d", temp->proc->od);
	}
	printf(">");
    return;
}

char* fprintQueue(QUEUE* queue){
    char* st = "";
	char* st2 = "";
	node *temp;

    strcat(st, "<");
    if(queue->front != NULL){
        for(temp = queue->front; temp->next != NULL; temp = temp->next){
            sprintf(st2, "%d, ", temp->proc->od);
			strcat(st, st2);
        }
        sprintf(st2, "%d", temp->proc->od);
		strcat(st, st2);
    }
    strcat(st, ">");
    return st;
}


node* find_proc(QUEUE* queue, int pid){
	node *temp;

	for(temp = queue->front; temp != NULL; temp = temp->next){
		if(temp->proc->pid == pid) return temp;
	}

	return NULL;
}

void changeq(QUEUE* a, QUEUE* b){
	a->front = b->front;
	a->rear = b->rear;
	a->count = b->count;

	b->front = NULL;
	b->rear = NULL;
	b->count = 0;
}
