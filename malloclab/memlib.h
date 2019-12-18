#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>

void mem_init(bool sparse);               
void mem_deinit(void);

/* 
 * mem_sbrk - simple model of the sbrk function. Extends the heap 
 *            by incr bytes and returns the start address of the new area. In
 *            this model, the heap cannot be shrunk.
 */
void *mem_sbrk(intptr_t incr);

/*
 * mem_reset_brk - reset the simulated brk pointer to make an empty heap
 */
void mem_reset_brk(void); 

/*
 * mem_heap_lo - return address of the first heap byte
 */
void *mem_heap_lo(void);

/* 
 * mem_heap_hi - return address of last heap byte
 */
void *mem_heap_hi(void);

size_t mem_heapsize(void);
size_t mem_pagesize(void);

/* Functions used for memory emulation */

/* Read len bytes and return value zero-extended to 64 bits */
/* Require 0 <= len <= 8 */
uint64_t mem_read(const void *addr, size_t len);

/* Write lower order len bytes of val to address */
/* Require 0 <= len <= 8 */
void mem_write(void *addr, uint64_t val, size_t len);

/* Emulation of memcpy */
void *mem_memcpy(void *dst, const void *src, size_t n);

/* Emulation of memset */
void *mem_memset(void *dst, int c, size_t n);

/* Debugging function to view region of heap */
void hprobe(void *ptr, int offset, size_t count);

/* Set whether the driver should check for UB */
void setUBCheck(bool);
