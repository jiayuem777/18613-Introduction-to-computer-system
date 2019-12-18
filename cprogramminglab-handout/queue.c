/* 
 * Code for basic C skills diagnostic.
 * Developed for courses 15-213/18-213/15-513 by R. E. Bryant, 2017
 * Modified to store strings, 2018
 */

/*
 * This program implements a queue supporting both FIFO and LIFO
 * operations.
 *
 * It uses a singly-linked list to represent the set of queue elements
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "queue.h"

/*
  Create empty queue.
  Return NULL if could not allocate space.
*/
queue_t *q_new()
{
    queue_t *q =  malloc(sizeof(queue_t));
    /* What if malloc returned NULL? */
    if (q == NULL){
      return NULL;
    }    
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
    return q;
}

/* Free all storage used by queue */
void q_free(queue_t *q)
{
    if(q == NULL) {
      return;
    }
    /* How about freeing the list elements and the strings? */
    list_ele_t *p;
    p = q->head;
    while(p != NULL){
      list_ele_t *temp;
      temp = p;
      p = p->next;
      free(temp->value);
      free(temp);
    }
    /* Free queue structure */
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
    free(q);
}

/*
  Attempt to insert element at head of queue.
  Return true if successful.
  Return false if q is NULL or could not allocate space.
  Argument s points to the string to be stored.
  The function must explicitly allocate space and copy the string into it.
 */
bool q_insert_head(queue_t *q, char *s)
{
    if(q == NULL){
      return false;
    }
    list_ele_t *newh;
    /* What should you do if the q is NULL? */
    newh = malloc(sizeof(list_ele_t));
    /* Don't forget to allocate space for the string and copy it */
    /* What if either call to malloc returns NULL? */
    if (newh == NULL){
      return false;
    }
    //printf("%lu\n", sizeof(s));
    newh->value = (char*)malloc((strlen(s) + 1) * sizeof(char));
    if(newh->value == NULL){
      free(newh);
      return false;
    }
    strcpy(newh->value, s);
    newh->next = q->head;
    q->head = newh;
    q->size += 1;
    if (q->size == 1){
      q->tail = q->head;
    }
    return true;
}


/*
  Attempt to insert element at tail of queue.
  Return true if successful.
  Return false if q is NULL or could not allocate space.
  Argument s points to the string to be stored.
  The function must explicitly allocate space and copy the string into it.
 */
bool q_insert_tail(queue_t *q, char *s)
{
    if (q == NULL){
      return false;
    }
    list_ele_t *newt;
    newt = malloc(sizeof(list_ele_t));
    if(newt == NULL){
      return false;
    }
    //printf("%lu\n", sizeof(s));
    newt->value = (char*)malloc((strlen(s) + 1) * sizeof(char));
    if(newt->value == NULL){
      free(newt);
      return false;
    }
    strcpy(newt->value,s);
    newt->next = NULL;
    if (q->size == 0){
      q->tail = newt;
      q->head = q->tail;
    }else{
    q->tail->next = newt;
    q->tail = newt;
    }
    q->size += 1;
    /* You need to write the complete code for this function */
    /* Remember: It should operate in O(1) time */
    return true;
}

/*
  Attempt to remove element from head of queue.
  Return true if successful.
  Return false if queue is NULL or empty.
  If sp is non-NULL and an element is removed, copy the removed string to *sp
  (up to a maximum of bufsize-1 characters, plus a null terminator.)
  The space used by the list element and the string should be freed.
*/
bool q_remove_head(queue_t *q, char *sp, size_t bufsize)
{
    /* You need to fix up this code. */
  if (q == NULL || q->size == 0){
    return false;
  }
    list_ele_t *temp;
    temp = q->head;
    //printf("%lu\n", sizeof(sp));
    if (sp != NULL){
      int len = strlen(q->head->value);
      if (len <= bufsize - 1){
        strcpy(sp, q->head->value);
      }else{
        char *to, *from;
        to = sp;
        from = q->head->value;
        int i = 0;
        for (i = 0; i < bufsize - 1; i++){
          *to = *from;
          to++;
          from++;
        }
        *to = '\0';
      }
    }
    q->head = q->head->next;
    q->size -= 1;
    if (q->size == 0){
      q->head = NULL;
      q->tail = NULL;
    }
    free(temp->value);
    free(temp);
    return true;
}

/*
  Return number of elements in queue.
  Return 0 if q is NULL or empty
 */
int q_size(queue_t *q)
{
    /* You need to write the code for this function */
    /* Remember: It should operate in O(1) time */
    if(q == NULL){
      return 0;
    }
    return q->size;
}

/*
  Reverse elements in queue
  No effect if q is NULL or empty
  This function should not allocate or free any list elements
  (e.g., by calling q_insert_head, q_insert_tail, or q_remove_head).
  It should rearrange the existing ones.
 */
void q_reverse(queue_t *q)
{
    /* You need to write the code for this function */
    if(q == NULL) {
      return;
    }
    list_ele_t *ptr;
    list_ele_t *temp;

    ptr = q->head;
    q->tail = q->head;
    q->head = NULL;
    while(ptr != NULL){
      temp = ptr->next;
      ptr->next = q->head;
      q->head = ptr;
      ptr = temp;
    }

}

