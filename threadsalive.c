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
	free(current_thread);
	swapcontext(&(current_thread -> context), &(next_to_run -> context));		//swap from current context to next context
    }
    swapcontext(&(current_thread -> context), &mainthread);
    return;
}

int ta_waitall(void) {
    while (head != NULL){
	swapcontext(&mainthread, &(head -> context));
	struct node *temp = head;
	head = head -> next;
	free(temp);
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
    struct node *unblock = *(sem -> blocked);
    if (sem -> blocked != NULL) {
	sem -> blocked = &(unblock -> next);
	list_append(unblock -> context, &head);
	free(unblock);					//Not sure if this is needed
	all_queues_empty--;				//decrement counter of total waiting/blocked queues
    }
}


void ta_sem_wait(tasem_t *sem) {
    sem -> count--;
    //if count < 0, wait in queue
    if (sem -> count < 0){
    	list_append((head -> context), (sem -> blocked));		//add the current thread to the end of the blocked queue
	all_queues_empty++;						//increment the count of blocked/waiting threads
	ta_yield();
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
    if (mutex -> lock == 1) {		//If some other thread has the lock, add current thread to waiting queue
    	list_append((head -> context), (mutex -> waiting));		//add the current thread to the end of the blocked queue
	all_queues_empty++;						//increment the count of blocked/waiting threads
	ta_yield();
    }
    else {mutex -> lock = 1;}			//Else, acquire the lock and keep running
}

void ta_unlock(talock_t *mutex) {
    struct node *ready = *(mutex -> waiting);
    if (ready == NULL) {				//If no threads are waiting for the lock, unlock the lock
	mutex -> lock = 0;
    }
    if (ready != NULL) {		//If there are threads waiting for the lock
	mutex -> waiting = &(ready -> next);
	list_append(ready -> context, &head);		//pass the lock to the next thread and add that thread to the ready queue
	free(ready);
	all_queues_empty--;
    }
    
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

