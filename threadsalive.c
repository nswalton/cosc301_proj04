/*
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <strings.h>
#include <string.h>
#include <ucontext.h>			//added for linked list queue

#include "threadsalive.h"

/* ***************************** 
     stage 1 library functions
   ***************************** */

void list_append(ucontext_t context, struct node **head) {
    struct node *appendnode = malloc(sizeof(struct node));

    appendnode -> context = context;
    appendnode -> next = NULL;
    if (*head == NULL) {
	*head = appendnode;
    }
    
    else {
	struct node *tempnode = *head;
	while (tempnode -> next != NULL){
	    tempnode = tempnode -> next;
	}
	tempnode -> next = appendnode;
    }
}


void list_destroy(struct node **head) {
    struct node *list = *head;
    struct node *temp;
    while (list != NULL) {
	temp = list -> next;
	free(list);
	list = temp;
    }
}


struct node *head;		//global head of thread queue
ucontext_t mainthread;		//global main context
int all_queues_empty = 0;	//count showing whether or not all waiting threads have run. 0 = true, >0 = false

void ta_libinit(void) {
    //head = malloc(sizeof(struct node));		//Don't need this since head lives in global memory
    return;
}

void ta_create(void (*func)(void *), void *arg) {
    unsigned char *stack = (unsigned char *)malloc(128000);
    ucontext_t newcontext;
    getcontext(&newcontext);
    newcontext.uc_stack.ss_sp = stack;
    newcontext.uc_stack.ss_size = 128000;
    newcontext.uc_link = &mainthread;
    makecontext(&newcontext, (void (*)(void))func, 1, arg); 
    list_append(newcontext, &head);

    return;
}


void ta_yield(void) {
    struct node *current_thread = head;
    if (head -> next != NULL) {					//If there is another thread waiting
	struct node *next_to_run;				//next_to_run is the node that contains the next context
	next_to_run = head -> next;
	list_append(current_thread -> context, &head);		//append the currently running thread to end of queue
	head = next_to_run;					//new head of queue is the context we are about to switch to
	swapcontext(&(current_thread -> context), &(next_to_run -> context));		//swap from current context to next context
    }
    return;
}

int ta_waitall(void) {
    while (head != NULL){
	swapcontext(&mainthread, &(head -> context));
	//deallocate the node at head
	head = head -> next;
    }
    if (all_queues_empty == 0){
	return 0;
    }
    return -1;
}


/* ***************************** 
     stage 2 library functions
   ***************************** */

void ta_sem_init(tasem_t *sem, int value) {
    sem -> count = value;
    sem -> blocked = NULL;
}

void ta_sem_destroy(tasem_t *sem) {
    list_destroy((sem -> blocked));
    free(sem -> blocked);
}

void ta_sem_post(tasem_t *sem) {
    sem -> count++;
    //unblock thread at head of queue (if there is one)
    all_queues_empty--; //do this only if a thread is actually unblocked
}

void ta_sem_wait(tasem_t *sem) {
    sem -> count--;
    //if count < 0, wait in queue
    if (sem -> count < 0){
	/*struct node *tempnode = head;
    	if (tempnode -> next != NULL) {							//if there is a thread waiting in the queue
	    struct node *currthread = tempnode -> next;
    	    list_append((tempnode -> context), (sem -> blocked));					//add current to end of queue
	    head = currthread;
        }*/
	all_queues_empty++;
    }
}

/*void ta_lock_init(talock_t *mutex) {			//For mutex, just use a semapohore and set the initial count to 1.
    ta_sem_init(mutex -> lock, 1);
}

void ta_lock_destroy(talock_t *mutex) {
    ta_sem_destroy(mutex -> lock);
}

void ta_lock(talock_t *mutex) {
    ta_sem_wait(mutex -> lock);
}

void ta_unlock(talock_t *mutex) {
    ta_sem_post(mutex -> lock);
}*/

void ta_lock_init(talock_t *mutex) {
    mutex -> lock = 0;
    mutex -> waiting = NULL;
}

void ta_lock_destroy(talock_t *mutex) {
    list_destroy(mutex -> waiting);
    free(mutex -> waiting);
}

void ta_lock(talock_t *mutex){
    if (mutex -> lock == 1) {
	all_queues_empty++;
    }
    mutex -> lock = 1;
}

void ta_unlock(talock_t *mutex) {
    mutex -> lock = 0;
}


/* ***************************** 
     stage 3 library functions
   ***************************** */

void ta_cond_init(tacond_t *cond) {
}

void ta_cond_destroy(tacond_t *cond) {
}

void ta_wait(talock_t *mutex, tacond_t *cond) {
}

void ta_signal(tacond_t *cond) {
}

