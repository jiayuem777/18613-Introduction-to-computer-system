/* 
 * @file cache.h
 * created by Jiayue Mao (Andrew ID: jiayuem)
 * This header file consists of the structs 
 * and functions used to build the cache.
 */

#include "csapp.h"
#include <stddef.h>                     /* size_t */
#include <sys/types.h>                  /* ssize_t */
#include <stdarg.h>                     /* va_list */
#include <pthread.h>

/*
 * Max cache and object sizes。
 */
#define MAX_CACHE_SIZE (1024*1024)
#define MAX_OBJECT_SIZE (100*1024)

/* struct of a cache block */
typedef struct block {
	char url[MAXLINE];
	char content[MAX_OBJECT_SIZE];
	size_t content_size; // the actual size of content
	int refcnt; // counter of readers
	struct block *prev; // prev pointer (server for double linked list)
	struct block *next; // next pointer (server for double linked list)
} cache_block;

/* struct of the whole cache */
typedef struct {
	// the head pointer pointed to the first cache block in the linked list
	cache_block *head; 
	cache_block *tail; // the tail pointer
	size_t total_size; // total size of all the cache blocks
} Cache;

/*
 * cache functions to initialize, read from, write to cache blocks,
 * and insert cache blocks, move cache block to the first and evict cache.
 */
void init_cache();
cache_block *find_cache(char *uri);
void increref(cache_block *block);
void decreref(cache_block *block);
void move_to_first(cache_block *block);
void read_from_cache(cache_block *block, int connfd);
cache_block *evict_cache(size_t content_size);
cache_block *insert_block();
void write_to_cache(cache_block *block, char *url, 
	                char *content, size_t content_size);
void find_cache_to_write(char *content, char *url, size_t content_size);
void print_cache();
