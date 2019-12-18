/* 
 * @file cache.c
 * created by Jiayue Mao (Andrew ID: jiayuem)
 */

#include "cache.h"
#include "csapp.h"
#include <stdio.h>                      /* stderr */
#include <string.h>                     /* memset() */
#include <stdbool.h>                    /* bool */
#include <stdlib.h>                     /* abort() */
#include <stddef.h>                     /* ssize_t */
#include <stdint.h>                     /* intmax_t */
#include <stdarg.h>                     /* va_list */
#include <errno.h>                      /* errno */
#include <unistd.h>                     /* STDIN_FILENO */
#include <semaphore.h>                  /* sem_t */
#include <netdb.h>                      /* freeaddrinfo() */
#include <sys/types.h>                  /* struct sockaddr */
#include <sys/socket.h>                 /* struct sockaddr */
#include <signal.h>                     /* struct sigaction */

/* the whole cache (linked list) */
Cache cache;

/* mutex used to lock the cache */
pthread_mutex_t mutex;

/* 
 * init_cache - the initialization of the whole cache
 * args: none
 * return: none
 */
void init_cache() {
	cache.total_size = 0;
	cache.head = NULL;
	cache.tail = NULL;
	pthread_mutex_init(&mutex, NULL);
}

/* 
 * read_from_cache - when the request URL is found in the cache, 
 * read from the cache block and move the block to the first.
 * args: 
 * cache_block *block - the pointer pointed to the cache block 
 * whose URL equals with the request URL.
 * int connfd - the file descriptor of client
 * return: none
 */
void read_from_cache(cache_block *block, int connfd) {
	pthread_mutex_lock(&mutex); //lock the cache
	move_to_first(block); //move the block to the first
	increref(block); //increase the refcnt of that block
	pthread_mutex_unlock(&mutex); //unlock cache
	/* client read from cache */
	rio_writen(connfd, block->content, block->content_size);
	pthread_mutex_lock(&mutex); //lock cache
	decreref(block); //finish reading, decrease refcnt.
	pthread_mutex_unlock(&mutex); //unlock cache
}

/* 
 * find_cache - find if there is cache block in the cache whose
 * URL equals with the uri argument. 
 * If there is, return the address of that cache block,
 * else return null.
 * args: char *uri - the URL to be found through the whole cache
 * return: cache_block * - the address of found cache block
 */
cache_block *find_cache(char *uri) {
	cache_block *temp = cache.head;
	/* look through from head to tail */
	while (temp != NULL) {
		if (!strcmp(temp->url, uri)) {
			/* if URL of the block equals wirh uri, return */
			return temp;
		} 
		temp = temp->next;
	}
	return temp; //temp == NULL, not found
}

/* 
 * increref - increase the reference counter of that cache block
 * args: cache_block *block - the address of a cache block
 * return: none
 */
void increref(cache_block *block) {
	block->refcnt++;
}

/* 
 * decreref - decrease the reference counter of that cache block.
 * If the refcnt of that block is decreased to 0, 
 * it means that the block has been evicted and all the readers have finished,
 * then free that block
 * args: cache_block *block - the address of a cache block
 * return: none
 */
void decreref(cache_block *block) {
	block->refcnt--;
	if (block->refcnt == 0) {
		free(block);
	}
}

/* 
 * move_to_first - move a cache block to the first in the linked list.
 * args: cache_block *block - the address of a cache block
 * return: none
 */
void move_to_first(cache_block *block) {
	cache_block *block_prev = block->prev;
	cache_block *block_next = block->next;
	/* if prev pointer of the block is NULL,
	 * it means the block is already the first one */
	if (block_prev == NULL) {
		return;
	}
	block_prev->next = block_next;
	if (block_next != NULL) { //if block is not the last
		block_next->prev = block_prev;
	} else { //if block is the last one
		cache.tail = block_prev;
	}

	block->next = cache.head;
	cache.head->prev = block;
	block->prev = NULL;
	cache.head = block;
}

/* 
 * find_cache_to_write - If there's no cache block with the same URL,
 * we need to write a new block in the cache.
 * args: 
 * char *content - the contents need to be writen
 * char *url - the URL needs to be writen
 * size_t content_size - the content size
 * return: none
 */
void find_cache_to_write(char *content, char *url, size_t content_size) {
	cache_block *new_block;
	/* It total size plus the new content size is less than MAX_CACHE_SIZE, 
	 * we just need to insert a new block at first and write to it */
	if ((cache.total_size + content_size) < MAX_CACHE_SIZE) {
		new_block = insert_block();
		cache.total_size += content_size;
	} else { /* else we need to evict other block(s) */
		new_block = evict_cache(content_size);
	}
	write_to_cache(new_block, url, content, content_size);
}

/* 
 * insert_block - insert a new block to the first of the whole cache.
 * args: none
 * return: cache_block * - the address of the new inserted block.
 */
cache_block *insert_block() {
	cache_block *insert = malloc(sizeof(cache_block));
	if (insert == NULL) {
		return NULL;
	}
	insert->prev = NULL;
	if (cache.head == NULL) { // if there's no block in cache
		cache.head = insert;
		cache.tail = cache.head;
		insert->next = NULL;
	} else {
		insert->next = cache.head;
		cache.head->prev = insert;
		cache.head = insert;
	}
	return insert;
}

/* 
 * evict_cache - evict the cache block(s) from the tail of the linked list
 * and insert a new block at head of the cache
 * args: size_t content_size - the size of content to be writen into 
 * the new block (determining the number of evicted blocks)
 * return: cache_block * - the address of the new inserted block.
 */
cache_block *evict_cache(size_t content_size) {
	size_t evict_size = 0;
	cache_block *temp = cache.tail;
	
	/* evict blocks from the tail */
	/* the total evicted size should be larger than new content size */
	while (evict_size < content_size && temp != NULL) {
		cache_block *t = temp;
		evict_size += temp->content_size;
		temp = temp->prev;
		decreref(t);
	}
	if (temp == NULL) {
		cache_block *new_block = malloc(sizeof(cache_block));
		if (new_block == NULL) {
			sio_printf("malloc error\n");
			return NULL;
		}
		cache.total_size = content_size;
		cache.head = new_block;
		cache.tail = new_block;
		new_block->prev = NULL;
		new_block->next = NULL;
		return new_block;
	} else {
		cache.total_size -= (evict_size - content_size);
		cache_block *new_block = malloc(sizeof(cache_block));
		if (new_block == NULL) {
			sio_printf("malloc error\n");
			return NULL;
		}
		temp->next = NULL;
		cache.tail = temp;
		new_block->next = cache.head;
		new_block->prev = NULL;
		cache.head = new_block;
		return new_block;
	}
}

/* 
 * write_to_cache - write the url and content to the new block
 * args: 
 * cache_block *block - the address of cache block to be written
 * char *url - the URL to be writen
 * char *content - the content to be written
 * size_t content_size - the actual size of the content
 * return: none
 */
void write_to_cache(cache_block *block, char *url, 
	                char *content, size_t content_size) {
	if (block == NULL) {
		return;
	}
	strcpy(block->url, url);
	memcpy(block->content, content, content_size);
	block->content_size = content_size;
	block->refcnt = 1;
}

/* 
 * print_block - print the whole cache
 * args: none
 * return: none
 */
void print_cache() {
	pthread_mutex_lock(&mutex);
	sio_printf("\nStart printing cache objects......\n");
	cache_block *temp = cache.head;
	while (temp != NULL) {
		sio_printf("**********************\n");
		sio_printf("url: %s\n", temp->url);
		sio_printf("content size: %d\n", (int)temp->content_size);
		sio_printf("reference count: %d\n", temp->refcnt);
		temp = temp->next;
	}
	sio_printf("\ntotal size: %d\n", (int)cache.total_size);
	pthread_mutex_unlock(&mutex);
}
