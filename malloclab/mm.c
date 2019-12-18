/*
 ******************************************************************************
 *                                   mm.c                                     *
 *           64-bit struct-based implicit free list memory allocator          *
 *                  15-213: Introduction to Computer Systems                  *
 *                                                                            *
 *  ************************************************************************  *
 *                  TODO: insert your documentation here. :)                  *
 *  
 *  Name: Jiayue Mao
 *  Andrew ID: jiayuem
 *    
 *  1. Use segerageted list to hold free blocks.
 *  2. Use LIFO to insert new free block in each list.
 *  3. Use first fit to search for a free block when mallocing.
 *  4. Remove footer of allocated blocks. The allocated blocks only have
 *  header (block size, prev block allocated, this block allocated). 
 *  Free blocks have header, prev pointer, next pointer and footer.                                                                    
 *  ************************************************************************  *
 *  ** ADVICE FOR STUDENTS. **                                                *
 *  Step 0: Please read the writeup!                                          *
 *  Step 1: Write your heap checker. Write. Heap. checker.                    *
 *  Step 2: Place your contracts / debugging assert statements.               *
 *  Good luck, and have fun!                                                  *
 *                                                                            *
 ******************************************************************************
 */

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "memlib.h"
#include "mm.h"

/* Do not change the following! */

#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mem_memset
#define memcpy mem_memcpy
#endif /* def DRIVER */

// the size of segregated lists
#define SIZE 15

//
#define RANGE 9

/* You can change anything from here onward */

/*
 * If DEBUG is defined (such as when running mdriver-dbg), these macros
 * are enabled. You can use them to print debugging output and to check
 * contracts only in debug mode.
 *
 * Only debugging macros with names beginning "dbg_" are allowed.
 * You may not define any other macros having arguments.
 */
#ifdef DEBUG
/* When DEBUG is defined, these form aliases to useful functions */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_requires(expr) assert(expr)
#define dbg_assert(expr) assert(expr)
#define dbg_ensures(expr) assert(expr)
#define dbg_printheap(...) print_heap(__VA_ARGS__)
#else
/* When DEBUG is not defined, no code gets generated for these */
/* The sizeof() hack is used to avoid "unused variable" warnings */
#define dbg_printf(...) (sizeof(__VA_ARGS__), -1)
#define dbg_requires(expr) (sizeof(expr), 1)
#define dbg_assert(expr) (sizeof(expr), 1)
#define dbg_ensures(expr) (sizeof(expr), 1)
#define dbg_printheap(...) ((void)sizeof(__VA_ARGS__))
#endif

/* Basic constants */

typedef uint64_t word_t;

// Word and header size (bytes)
static const size_t wsize = sizeof(word_t);

// Double word size (bytes)
static const size_t dsize = 2 * wsize;

// Minimum block size (bytes)
static const size_t min_block_size = 2 * dsize;

// TODO: explain what chunksize is
// (Must be divisible by dsize)
static const size_t chunksize = (1 << 12);

// TODO: explain what alloc_mask is
static const word_t alloc_mask = 0x1;

static const word_t prev_alloc_mask = 0x2;

static const word_t prev_16B_mask = 0x4;

// TODO: explain what size_mask is
static const word_t size_mask = ~(word_t)0xF;

/* Represents the header and payload of one block in the heap */
typedef struct block {
  /* Header contains size + allocation flag */
  word_t header;

  /*
   * TODO: feel free to delete this comment once you've read it carefully.
   * We don't know what the size of the payload will be, so we will declare
   * it as a zero-length array, which is a GCC compiler extension. This will
   * allow us to obtain a pointer to the start of the payload.
   *
   * WARNING: A zero-length array must be the last element in a struct, so
   * there should not be any struct fields after it. For this lab, we will
   * allow you to include a zero-length array in a union, as long as the
   * union is the last field in its containing struct. However, this is
   * compiler-specific behavior and should be avoided in general.
   *
   * WARNING: DO NOT cast this pointer to/from other types! Instead, you
   * should use a union to alias this zero-length array with another struct,
   * in order to store additional types of data in the payload memory.
   */
  char payload[0];

  /*
   * TODO: delete or replace this comment once you've thought about it.
   * Why can't we declare the block footer here as part of the struct?
   * Why do we even have footers -- will the code work fine without them?
   * which functions actually use the data contained in footers?
   */
} block_t;

/* Global variables */
static block_t *heap_start;
static block_t *root_list[SIZE];

/* Function prototypes for internal helper routines */

bool mm_checkheap(int lineno);

static block_t *extend_heap(size_t size);
static block_t *find_fit(size_t asize);
static block_t *coalesce_block(block_t *block);
static void split_block(block_t *block, size_t asize, size_t block_size);

static size_t max(size_t x, size_t y);
static size_t round_up(size_t size, size_t n);
static word_t pack(size_t size, bool alloc, bool prev, bool prev_16B);

static size_t extract_size(word_t header);
static size_t get_size(block_t *block);
static word_t get_content(void *p);
static size_t get_payload_size(block_t *block);

static bool extract_alloc(word_t header);
static bool get_alloc(block_t *block);
static bool get_prev_alloc(block_t *block);
static bool get_prev_16B(block_t *block);

static void write_header(block_t *block, size_t size, bool alloc, bool prev, bool prev_16B);
static void write_footer(block_t *block, size_t size, bool alloc, bool prev, bool prev_16B);

static void *prev_linknode(void *bp);
static void *next_linknode(void *bp);

static void put(void *p, uint64_t val);
static block_t *get(void *p);

static block_t *payload_to_header(void *bp);
static void *header_to_payload(block_t *block);
static word_t *header_to_footer(block_t *block);

static block_t *find_next(block_t *block);
static word_t *find_prev_footer(block_t *block);
static block_t *find_prev(block_t *block);

static void set_prev_alloc(block_t *block, bool prev_alloc);
static void set_prev_16B(block_t *block, bool prev_16B);

static void insert_node(block_t *root_item[SIZE], block_t *node);
static void delete_node(block_t *root_list[SIZE], block_t *node);
static int find_listnumber(size_t size);

/*
 * mm_init: to initialize a heap.
 * args: none
 * return: 
 * true if the initialization is successful, otherwise false.
 */
bool mm_init(void) {
  // Create the initial empty heap
  word_t *start = (word_t *)(mem_sbrk(2 * wsize));

  if (start == (void *)-1) {
    return false;
  }
  int i = 0;
  for (i = 0; i < SIZE; i++) {
    root_list[i] = 0;
  }

  start[0] = pack(0, true, true, false); // Heap prologue (block footer)
  start[1] = pack(0, true, false, false); // Heap epilogue (block header)

  // Heap starts with first "block header", currently the epilogue
  heap_start = (block_t *)&start[1];
  root_list[SIZE - 1] = (block_t *)&start[1];

  // Extend the empty heap with a free block of chunksize bytes
  if (extend_heap(chunksize) == NULL) {
    return false;
  }

  return true;
}

/*
 * malloc: to malloc payload in the heap
 * args: 
 * size_t size: the size of input
 * return: the payload address of a allocated block
 */
void *malloc(size_t size) {
  dbg_requires(mm_checkheap(__LINE__));

  size_t asize;      // Adjusted block size
  size_t extendsize; // Amount to extend heap if no fit is found
  block_t *block;
  void *bp = NULL;

  bool result = (root_list[0] == NULL);

  for (int i = 1; i < SIZE; i++) {
  	result = result && (root_list[i] == NULL);
  }

  if (result) // Initialize heap if it isn't initialized
  {
    mm_init();
  }

  if (size == 0) // Ignore spurious request
  {
    dbg_ensures(mm_checkheap(__LINE__));
    return bp;
  }

  // Adjust block size to include overhead and to meet alignment requirements
  asize = round_up(size + wsize, dsize);

  // Search the free list for a fit
  block = find_fit(asize);

  // If no fit is found, request more memory, and then and place the block
  if (block == NULL) {
    // Always request at least chunksize
    extendsize = max(asize, chunksize);

    block = extend_heap(extendsize);
    if (block == NULL) // extend_heap returns an error
    {
      return bp;
    }
  }

  size_t block_size = get_size(block);

  // The block should be marked as free
  dbg_assert(!get_alloc(block));

  // Mark block as allocated
  write_header(block, block_size, true, true, get_prev_16B(block));

  // Try to split the block if too large
  split_block(block, asize, block_size);

  bp = header_to_payload(block);

  dbg_ensures(mm_checkheap(__LINE__));
  return bp;
}

/*
 * free: to free a block in the heap.
 * args: 
 * void *bp: the address of payload in the block
 * return: void
 */
void free(void *bp) {
  dbg_requires(mm_checkheap(__LINE__));

  if (bp == NULL) {
    return;
  }

  block_t *block = payload_to_header(bp);
  size_t size = get_size(block);

  // The block should be marked as allocated
  dbg_assert(get_alloc(block));

  //check if the prev block is allocated to set the prev_alloc
  bool prev_alloc = get_prev_alloc(block);

  // Mark the block as free
  write_header(block, size, false, prev_alloc, get_prev_16B(block));
  if (size >= min_block_size) {
    write_footer(block, size, false, prev_alloc, get_prev_16B(block));
  	put(prev_linknode(bp), 0);
  	put(next_linknode(bp), 0);
  } else {
    put(next_linknode(bp), 0);
  }

  // Try to coalesce the block with its neighbors
  block = coalesce_block(block);

  dbg_ensures(mm_checkheap(__LINE__));
}

/*
 * realloc: realloc size-byte payload of an allocated block to a new block.
 * args:
 * void *ptr: the payload address of a malloced block.
 * size_t size: the size of payload to realloc
 * return: a new payload address after realloc
 */
void *realloc(void *ptr, size_t size) {
  block_t *block = payload_to_header(ptr);
  size_t copysize;
  void *newptr;

  // If size == 0, then free block and return NULL
  if (size == 0) {
    free(ptr);
    return NULL;
  }

  // If ptr is NULL, then equivalent to malloc
  if (ptr == NULL) {
    return malloc(size);
  }

  // Otherwise, proceed with reallocation
  newptr = malloc(size);

  // If malloc fails, the original block is left untouched
  if (newptr == NULL) {
    return NULL;
  }

  // Copy the old data
  copysize = get_payload_size(block); // gets size of old payload
  if (size < copysize) {
    copysize = size;
  }
  memcpy(newptr, ptr, copysize);

  // Free the old block
  free(ptr);

  return newptr;
}

/*
 * calloc: Allocates memory for an array of nmemb elements of size bytes each and returns a pointer to the allocated memory.
 * The memory is set to zero before returning.
 * args:
 * size_t elements: the number of elements
 * size_t size: the size of each element
 * return: a pointer pointed to the payload of allocated block.
 */
void *calloc(size_t elements, size_t size) {
  void *bp;
  size_t asize = elements * size;

  if (asize / elements != size) {
    // Multiplication overflowed
    return NULL;
  }

  bp = malloc(asize);
  if (bp == NULL) {
    return NULL;
  }

  // Initialize all bits to 0
  memset(bp, 0, asize);

  return bp;
}

/******** The remaining content below are helper and debug routines ********/

/*
 * extend_heap: to extend the heap size when the heap is initialized or there is no block fit.
 * args:
 * size_t size: the size to extend
 * return:
 * a block_t pointer pointed to the extended block (after coalescing)
 */
static block_t *extend_heap(size_t size) {
  void *bp;

  // Allocate an even number of words to maintain alignment
  size = round_up(size, dsize);
  if ((bp = mem_sbrk(size)) == (void *)-1) {
    return NULL;
  }

  // Initialize free block header/footer
  block_t *block = payload_to_header(bp);

  //check if the prev block is allocated
  bool prev_alloc = get_prev_alloc(block);

  bool prev_16B = get_prev_16B(block);

  // if the prev block is epilogue block then prev_alloc is true
  if (!prev_16B && extract_size(*((word_t *)block - 1)) == 0) {
  	prev_alloc = true;
    prev_16B = false;
  }
  
  //write header and footer of the extended block
  write_header(block, size, false, prev_alloc, prev_16B);
  write_footer(block, size, false, prev_alloc, prev_16B);
  //set the prev and next pointer of the extended block to be null.
  put(prev_linknode(bp), 0);
  put(next_linknode(bp), 0);
  
  // Create new epilogue header
  block_t *block_next = find_next(block);
  write_header(block_next, 0, true, false, false);

  // Coalesce in case the previous block was free
  block = coalesce_block(block);

  return block;
}

/*
 * coalesce_block: to coalesce when the next or prev block of the current block is also free when free or extend heap.
 * args:
 * block_t *block: a block_t pointer pointed to the block that need to be coalesced by looking at both its prev and next block.
 * return:
 * a new block_t pointer pointed to the free block after coalescing.
 */
static block_t *coalesce_block(block_t *block) {
  dbg_requires(!get_alloc(block));

  size_t size = get_size(block);

  block_t *block_next = find_next(block);

  bool prev_alloc = get_prev_alloc(block);
  bool next_alloc = get_alloc(block_next);

  if (prev_alloc && next_alloc) // Case 1
  {
    // insert the new free block into the seglist.
 	  insert_node(root_list, block);
    set_prev_alloc(block_next, false);
  }

  else if (prev_alloc && !next_alloc) // Case 2
  {
    size += get_size(block_next);
    
    delete_node(root_list, block_next);
    write_header(block, size, false, true, get_prev_16B(block));
    write_footer(block, size, false, true, get_prev_16B(block));
    insert_node(root_list, block);
    block_t *block_next_next = find_next(block);
    set_prev_16B(block_next_next, false);
  }

  else if (!prev_alloc && next_alloc) // Case 3
  {

    block_t *block_prev;
    if (get_prev_16B(block)) {
      block_prev = (block_t *)((word_t *)block - 2);
    } else {
      block_prev = find_prev(block);
    }

    size += get_size(block_prev);

    delete_node(root_list, block_prev);
    write_header(block_prev, size, false, true, get_prev_16B(block_prev));
    write_footer(block_prev, size, false, true, get_prev_16B(block_prev));
    block = block_prev;
    set_prev_alloc(block_next, false);
    set_prev_16B(block_next, false);
    insert_node(root_list, block);
  }

  else // Case 4
  {
    block_t *block_prev;
    if (get_prev_16B(block)) {
      block_prev = (block_t *)((word_t *)block - 2);
    } else {
      block_prev = find_prev(block);
    }
  	size += get_size(block_next) + get_size(block_prev); //size after coalescing both prev and next
  	// delete both prev and next from list
  	delete_node(root_list, block_prev);
    delete_node(root_list, block_next);
    //create the new block and insert to the list
    write_header(block_prev, size, false, true, get_prev_16B(block_prev));
    write_footer(block_prev, size, false, true, get_prev_16B(block_prev));
    block = block_prev;
    insert_node(root_list, block);
    block_t *block_next_next = find_next(block);
    set_prev_16B(block_next_next, false);
  }

  dbg_ensures(!get_alloc(block));

  return block;
}

/*
 * split_block: to split a block if the block found to malloc is too large
 * args: 
 * block_t *block: the pointer pointed to the block that need to be splited.
 * size_t asize: the malloc size
 * size_t block_size: the block size
 * return: void
 */
static void split_block(block_t *block, size_t asize, size_t block_size) {
  dbg_requires(get_alloc(block));
  
  if ((block_size - asize) >= dsize) {
    
    //delete the current block from list
    delete_node(root_list, block);
    //write the header of an allocated block
    write_header(block, asize, true, true, get_prev_16B(block));
    
    //write the header and footer of the next block. prev_alloc = true
    size_t next_size = block_size - asize;
    block_t *block_next = find_next(block);

    bool prev_16B = false;
    if (asize == dsize) {
      prev_16B = true;
    }
    write_header(block_next, next_size, false, true, prev_16B);
    write_footer(block_next, next_size, false, true, prev_16B);

    block_t *block_next_next = find_next(find_next(block));
    if (next_size == dsize) {
      set_prev_16B(block_next_next, true);
    } 

    //insert next block to the list
    insert_node(root_list, block_next);

  } else {
  	//delete the current block from the list.
    delete_node(root_list, block);
    write_header(block, block_size, true, true, get_prev_16B(block));

    //set the prev_alloc of the next block to be true.
    block_t *block_next = find_next(block);
    set_prev_alloc(block_next, true);  
  }

  dbg_ensures(get_alloc(block));
}

/*
 * find_fit: find a suitable block for a speicific size malloc.
 * size_t asize: the size to be malloc
 * return block_t *: the block_t pointer pointed to a chosen block.
 */

static block_t *find_fit(size_t asize) {
  block_t *chosen_blocks[RANGE];
  int i = 0;
  for (i = 0; i < RANGE; i++) {
    chosen_blocks[i] = 0;
  }
  int listnumber = find_listnumber(asize);
  block_t *block;
  i = 0;
  //combined fit
  for (int n = listnumber; (n < SIZE) && (i < RANGE); n++) {
    for (block = root_list[n]; (block != NULL) && (i < RANGE); block = get(next_linknode(header_to_payload(block)))) {
      if (!(get_alloc(block)) && (asize <= get_size(block))) { 
        chosen_blocks[i] = block;
        i += 1;
      }
    }
  }

  block_t *best = chosen_blocks[0];
  if (best != NULL) {
    for (i = 0; i < RANGE; i++) {
      if (chosen_blocks[i] != NULL) {
        if ((get_size(chosen_blocks[i]) - asize) < (get_size(best) - asize)) {
          best = chosen_blocks[i];
        }
      }
    }
  }

  return best; 
}



/*
 * find the address used to holding prev pointer in a free block
 * args: 
 * void *bp: the payload address of a block
 * return: void
 */
static void *prev_linknode(void *bp) {
  return (void *)((unsigned char *)bp + wsize);
}

/*
 * find the address used to hold next pointer in a free block
 * args:
 * void *bp: the payload address of a block
 * return: void
 */
static void *next_linknode(void *bp) {
  return bp;
}

/*
 * put a 64-bit value in an address
 * args:
 * void *p: the address
 * uint64_t val: the 64-bit value
 * return void
 */
static void put(void *p, uint64_t val) {
  *((word_t *) p) = val;
}

/*
 * get the 8-byte content in address
 * args: 
 * void *p: the address
 * return: 
 * block_t *: the content (a block_t pointer)
 * this function is used to get the content in prev, next pointer, thus the output is a block_t pointer.
 */
static block_t *get(void *p) {
  return (block_t *) (*(word_t *)(p));
}

/*
 * set the prev_alloc bit in a block.
 * args: 
 * block_t *block: the block address
 * bool prev alloc: whether the prev block is allocated
 * return void
 */
static void set_prev_alloc(block_t *block, bool prev_alloc) {
  word_t header = block->header;
  if (prev_alloc) {
  	header = (header | prev_alloc_mask);
  } else {
  	header = (header & (~prev_alloc_mask));
  }
  block->header = header;
}

/*
 * set the prev_16B bit in a block.
 * args: 
 * block_t *block: the block address
 * bool prev alloc: whether the prev block is allocated
 * return void
 */
static void set_prev_16B(block_t *block, bool prev_16B) {
  word_t header = block->header;
  if (prev_16B) {
    header = (header | prev_16B_mask);
  } else {
    header = (header & (~prev_16B_mask));
  }
  block->header = header;
}

/*
 * find the number to determine which list to choose.
 * args: 
 * size_t size: the size of a block
 * return: the number of a chosen list
 * precondition: when insert or delete a block in the list.
 */
static int find_listnumber(size_t size) {
	int listnumber = 0;
  if (size == dsize) {
    listnumber = 0;
  } else if (size == dsize * 2) {
		listnumber = 1;
	} else if (size >= dsize * 3 && size <= dsize * 4) {
		listnumber = 2;
	} else if (size >= dsize * 5 && size <= dsize * 6) {
		listnumber = 3;
	} else if (size >= dsize * 7 && size <= dsize * 9) {
		listnumber = 4;
	} else if (size >= dsize * 10 && size <= dsize * 12) {
		listnumber = 5;
	} else if (size >= dsize * 13 && size <= dsize * 15) {
		listnumber = 6;
	} else if (size >= dsize * 16 && size <= dsize * 20) {
		listnumber = 7;
	} else if (size >= dsize * 21 && size <= dsize * 26) {
		listnumber = 8;
	} else if (size >= dsize * 27 && size <= dsize * 36) {
		listnumber = 9;
	} else if (size >= dsize * 37 && size <= dsize * 50) {
    listnumber = 10;
	} else if (size >= dsize * 51 && size <= dsize * 70) {
    listnumber = 11;
  } else if (size >= dsize * 71 && size <= dsize * 99) {
    listnumber = 12;
  } else if (size >= dsize * 100 && size <= dsize * 140) {
    listnumber = 13;
  } else if (size >= dsize * 141) {
    listnumber = 14;
  }
	return listnumber;
}

/*
 * to insert a block in the seglist
 * args: 
 * block_t *root_list[SIZE]: seglist
 * block_t *node: the address of the block need to be inserted.
 * return void
 */
static void insert_node(block_t *root_list[SIZE], block_t *node) {
	
  size_t node_size = get_size(node);
	int listnumber = find_listnumber(node_size);
  
  block_t *old_head = root_list[listnumber];
  if (old_head == node) {
    return;
  }

  // 16B block only have next pointer
  if (listnumber == 0) {
    root_list[listnumber] = node;
    put(next_linknode(header_to_payload(node)), (word_t)old_head);
    return;
  }

	put(next_linknode(header_to_payload(node)), (word_t)old_head);
	if (old_head != NULL) {
		put(prev_linknode(header_to_payload(old_head)), (word_t)node);
	}
	root_list[listnumber] = node;
	put(prev_linknode(header_to_payload(node)), 0);

}

/*
 * to delete a block in the seglist
 * args: 
 * block_t *root_list[SIZE]: seglist
 * block_t *node: the address of the block need to be deleted.
 * return void
 */
static void delete_node(block_t *root_list[SIZE], block_t *node) {
	size_t node_size = get_size(node);
	int listnumber = find_listnumber(node_size);

  // for 16B block: no prev pointer, get along the list to find the block to be deleted.
  if (listnumber == 0) {
    if (root_list[0] == node) {
      root_list[0] = get(next_linknode(header_to_payload(node)));
      return;
    }
    block_t *prev = root_list[0];
    if (prev == NULL) {
      return;
    }
    block_t *curr = get(next_linknode(header_to_payload(prev)));
    while (curr != NULL) {
      if (curr == node) {
        put(next_linknode(header_to_payload(prev)), (word_t)get(next_linknode(header_to_payload(curr))));
        return;
      }
      prev = curr;
      curr = get(next_linknode(header_to_payload(prev)));
    }
  } else {
    // if the block is the root block
    if (root_list[listnumber] == node) {
      block_t *node_next = get(next_linknode(header_to_payload(node)));
      root_list[listnumber] = node_next;
      if (node_next != NULL) {
        put(prev_linknode(header_to_payload(node_next)), 0);
      }

    } else {
      block_t *node_prev = get(prev_linknode(header_to_payload(node)));
      block_t *node_next = get(next_linknode(header_to_payload(node)));
      if (node_next != NULL) {
        put(prev_linknode(header_to_payload(node_next)), (word_t)node_prev);
      }
      put(next_linknode(header_to_payload(node_prev)), (word_t)node_next);
    }

  }
}

/*
 * The heap checker scans the heap and checks it for possible errors 
 * args:
 * int line: the number of line calling this function.
 * return: true if no error, false otherwise.
 */
bool mm_checkheap(int line) {
   /*
    * - Checking the heap (implicit list, explicit list, segregated list):
	∗ 1. Check epilogue and prologue blocks.
	∗ 2. Check each block’s address alignment.
	∗ 3. Check heap boundaries.
	∗ 4. Check each block’s header and footer: size (minimum size), previous/next allocate/free bit
	* consistency, header and footer matching each other.
	∗ 5. Check coalescing: no two consecutive free blocks in the heap.
	* 
	* – Checking the free list (explicit list, segregated list):
	∗ 1. All next/previous pointers are consistent (if A’s next pointer points to B, B’s previous
	* pointer should point to A).
	∗ 2. All free list pointers are between mem heap lo() and mem heap high().
	∗ 3. Count free blocks by iterating through every block and traversing free list by pointers and
	* see if they match.
	∗ 4. All blocks in each list bucket fall within bucket size range (segregated list)
	*/

	if (line == 0) {
		return true;
	}
	  
	// check epilogue block
	word_t *ep_block = (word_t *)mem_heap_lo();
	if (ep_block == NULL) {
		printf("line %d: The epilogue block is null.\n", line);
		return false;
	} else {
		size_t size = extract_size(*ep_block);
		if (size != 0) {
			printf("line %d: The size of the epilogue block is not 0.\n", line);
			return false;
		}
		bool alloc = extract_alloc(*ep_block);
		if (!alloc) {
			printf("line %d: the epilogue block is not allocated.\n", line);
			return false;
		}
	}

	// check prologue block
	word_t *pi_block = (word_t *)(mem_heap_hi() - 7);
	if (pi_block == NULL) {
		printf("line %d: The prologue block is null.\n", line);
		return false;
	} else {
		size_t size = extract_size(*pi_block);
		if (size != 0) {
			printf("line %d: The size of the prologue block is not 0.\n", line);
			return false;
		}
		bool alloc = extract_alloc(*pi_block);
		if (!alloc) {
			printf("line %d: the prologue block is not allocated.\n", line);
			return false;
		}
	}

	block_t *block;
	size_t size = 0;
	size_t free_count = 0;

	//iterate through every block in the heap.
	for (block = heap_start; block != (block_t *)pi_block; block = find_next(block)) {
		// check block address alignment.
		block_t *cal_address = (block_t *)((void *)heap_start + size);
		if (cal_address != block) {
			printf("line %d: the block address alignment is wrong. ", line);
			printf("The address should be 0x%lx, but the actual block address is 0x%lx.\n", (unsigned long)cal_address, (unsigned long)block);
			return false;
		}
		size += get_size(block);

		//check minimal block size
		if (get_size(block) < dsize) {
			printf("line %d: the size of the block is smaller than 16 bytes. ", line);
			printf("The block is 0x%lx.\n", (unsigned long)block);
			return false;
		}

		//check if the size of the block is multiple of 16.
		if ((get_size(block) % dsize) != 0) {
			printf("line %d: the size of the block is not multiple of 16 bytes. ", line);
			printf("The block is 0x%lx.\n", (unsigned long)block);
			return false;
		}


		//check heap boundaries
    //printf("heap start: %lx, pi block: %lx\n", (unsigned long)heap_start, (unsigned long)pi_block);
		if ((word_t)block < (word_t)heap_start || (word_t)block > (word_t)pi_block) {
			printf("line %d: The block address is outside the heap boundaries. ", line);
			printf("The block is 0x%lx.\n", (unsigned long)block);
			return false;
		}

		//for free blocks.
		if (!get_alloc(block) && get_size(block) >= min_block_size) {
			
			//check header and footer
			word_t block_header = block->header;
			word_t block_footer = *(header_to_footer(block));
			if ((unsigned long) block_header != 1L) {
				if (block_header != block_footer) {
				  printf("line %d: block header not equal to footer.", line);
				  printf(" The block is 0x%lx\n", (unsigned long)block);
				  return false;
				}
			}

			//check prev_alloc and next_alloc (They should be both allocated)
			if (!get_prev_alloc(block)) {
				printf("line %d: The prev block of a free block is not allocated. ", line);
				printf(" The block is 0x%lx\n", (unsigned long)block);
				return false;
			}
			if (!get_alloc(find_next(block))) {
				printf("line %d: The next block of a free block is not allocated. ", line);
				printf(" The block is 0x%lx\n", (unsigned long)block);
				return false;
			}

			if (get_size(block) >= min_block_size) {
        free_count += 1;

				//check free list pointer boundaries
				block_t *next = get(next_linknode(header_to_payload(block)));
				block_t *prev = get(prev_linknode(header_to_payload(block)));
				if (next != NULL && ((word_t)next < (word_t)(ep_block + 1) || (word_t)next > (word_t)pi_block)) {
					printf("line %d: The next pointer is outside the heap boundaries. ", line);
					printf("The block is 0x%lx.\n", (unsigned long)block);
					return false;
				}
				if (prev != NULL && ((word_t)prev < (word_t)(ep_block + 1) || (word_t)prev > (word_t)pi_block)) {
					printf("line %d: The prev pointer is outside the heap boundaries. ", line);
					printf("The block is 0x%lx.\n", (unsigned long)block);
					return false;
				}

				//check free block consistency
				if (next != NULL) {
					block_t *next_prev = get(prev_linknode(header_to_payload(next)));
					if (next_prev != block) {
					  printf("line %d: the prev pointer of the next block does not point to this block.", line);
					  printf(" The block is 0x%lx\n", (unsigned long)block);
					  return false;
					}
				}
				if (prev != NULL) {
					block_t *prev_next = get(next_linknode(header_to_payload(prev)));
					if (prev_next != block) {
					  printf("line %d: the next pointer of the prev block does not point to this block.", line);
					  printf(" The block is 0x%lx\n", (unsigned long)block);
					  return false;
					}
				}

				//check if any block has null prev pointer when it's not a root block.
				bool is_root = false;
				if (prev == NULL) {
					int i = 0;
					for (i = 0; i < SIZE; i++) {
						if (block == root_list[i]) {
							is_root = true;
						}
					}
					if (!is_root) {
					  printf("line %d: not the root block has a NULL prev pointer.", line);
					  printf(" The block is 0x%lx\n", (unsigned long)block);
					  return false;
					}
				}

			}
		}
	}

	// the size of the heap
	size_t heap_size = mem_heapsize();

	//check is the heap size match
	if ((size + 16 + ((word_t)heap_start - (word_t)(ep_block + 1))) != heap_size) {
		printf("line %d: the total size of blocks does not match with the heap size.\n", line);
		return false;
	}

	// check free blocks in segregated lists
	// check free blocks in segregated lists
  size_t free_in_list_count = 0;
  for (int n = 1; n < SIZE; n++) {
    if (root_list[n] != NULL) {

      block_t *root = root_list[n];

        //check if the prev pointer of root block is null.
      if (get(prev_linknode(header_to_payload(root))) != 0) {
        printf("line %d: the prev pointer of No.%d root block is not NULL.\n", line, n);
        return false;
      }
      for (block = root; block != NULL; block = get(next_linknode(header_to_payload(block)))) {
        if ((word_t)block >= (word_t)heap_start) {
          free_in_list_count += 1;
        }

        //check if all blocks in each list bucket fall within bucket size range
        if (find_listnumber(get_size(block)) != n) {
          printf("line %d: the size of the block is not in the NO.%d list.", line, n);
          printf(" The block is 0x%lx\n", (unsigned long)block);
          return false;
        }

        //check if all the block in the list is free.
        if (get_alloc(block)) {
          printf("line: %d: An allocated block in the free list.", line);
          printf(" The block is 0x%lx\n", (unsigned long) block);
          return false;
        }
      }
    }
  }

    //Count free blocks by iterating through every block and traversing free list by pointers and see if they match.
  if (free_in_list_count != free_count) {
    printf("line %d: The total number of free blocks is not equal to the number of free blocks in the lists. \n", line);
    return false;
  }

  return true;
}

/*
 *****************************************************************************
 * The functions below are short wrapper functions to perform                *
 * bit manipulation, pointer arithmetic, and other helper operations.        *
 *                                                                           *
 * We've given you the function header comments for the functions below      *
 * to help you understand how this baseline code works.                      *
 *                                                                           *
 * Note that these function header comments are short since the functions    *
 * they are describing are short as well; you will need to provide           *
 * adequate details within your header comments for the functions above!     *
 *                                                                           *
 *                                                                           *
 * Do not delete the following super-secret(tm) lines!                       *
 *                                                                           *
 * 53 6f 20 79 6f 75 27 72 65 20 74 72 79 69 6e 67 20 74 6f 20               *
 *                                                                           *
 * 66 69 67 75 72 65 20 6f 75 74 20 77 68 61 74 20 74 68 65 20               *
 * 68 65 78 61 64 65 63 69 6d 61 6c 20 64 69 67 69 74 73 20 64               *
 * 6f 2e 2e 2e 20 68 61 68 61 68 61 21 20 41 53 43 49 49 20 69               *
 *                                                                           *
 * 73 6e 27 74 20 74 68 65 20 72 69 67 68 74 20 65 6e 63 6f 64               *
 * 69 6e 67 21 20 4e 69 63 65 20 74 72 79 2c 20 74 68 6f 75 67               *
 * 68 21 20 2d 44 72 2e 20 45 76 69 6c 0a de ba c1 e1 52 13 0a               *
 *                                                                           *
 *****************************************************************************
 */

/*
 * max: returns x if x > y, and y otherwise.
 */
static size_t max(size_t x, size_t y) { return (x > y) ? x : y; }

/*
 * round_up: Rounds size up to next multiple of n
 */
static size_t round_up(size_t size, size_t n) {
  return n * ((size + (n - 1)) / n);
}

/*
 * pack: returns a header reflecting a specified size and its alloc status.
 *       If the block is allocated, the lowest bit is set to 1, and 0 otherwise.
 */
static word_t pack(size_t size, bool alloc, bool prev, bool prev_16B) {
  // word_t p = 0;
  word_t a = alloc? alloc_mask : 0;
  word_t p = prev? prev_alloc_mask : 0;
  word_t p16 = prev_16B? prev_16B_mask : 0;
  return (size | a | p | p16);

}

/*
 * extract_size: returns the size of a given header value based on the header
 *               specification above.
 */
static size_t extract_size(word_t word) { return (word & size_mask); }

/*
 * get_size: returns the size of a given block by clearing the lowest 4 bits
 *           (as the heap is 16-byte aligned).
 */
static size_t get_size(block_t *block) { return extract_size(block->header); }

/*
 * get_payload_size: returns the payload size of a given block, equal to
 *                   the entire block size minus the header and footer sizes.
 */
static word_t get_payload_size(block_t *block) {
  size_t asize = get_size(block);
  return asize - wsize;
}

/*
 * extract_alloc: returns the allocation status of a given header value based
 *                on the header specification above.
 */
static bool extract_alloc(word_t word) { return (bool)(word & alloc_mask); }

/*
 * get_alloc: returns true when the block is allocated based on the
 *            block header's lowest bit, and false otherwise.
 */
static bool get_alloc(block_t *block) { return extract_alloc(block->header); }

/*
 * get_prev_alloc: returns true when the block before that block is allocated based on the
 *            block header's second lowest bit, and false otherwise.
 */
static bool get_prev_alloc(block_t *block) {
  return (bool)((block->header) & prev_alloc_mask);
  
}

/*
 * get_prev_16B: returns true when the block before that block is 16 bytes based on the
 *            block header's third lowest bit, and false otherwise.
 */
static bool get_prev_16B(block_t *block) {
  return (bool)((block->header) & prev_16B_mask);
}


/*
 * write_header: given a block and its size and allocation status,
 *               writes an appropriate value to the block header.
 * TODO: Are there any preconditions or postconditions?
 */
static void write_header(block_t *block, size_t size, bool alloc, bool prev, bool prev_16B) {
  dbg_requires(block != NULL);
  block->header = pack(size, alloc, prev, prev_16B);
}

/*
 * write_footer: given a block and its size and allocation status,
 *               writes an appropriate value to the block footer by first
 *               computing the position of the footer.
 * TODO: Are there any preconditions or postconditions?
 */
static void write_footer(block_t *block, size_t size, bool alloc, bool prev, bool prev_16B) {
  dbg_requires(block != NULL);
  dbg_requires(get_size(block) == size && size > 0);
  word_t *footerp = header_to_footer(block);
  *footerp = pack(size, alloc, prev, prev_16B);
}


/*
 * find_next: returns the next consecutive block on the heap by adding the
 *            size of the block.
 */
static block_t *find_next(block_t *block) {
  dbg_requires(block != NULL);
  dbg_requires(get_size(block) != 0);
  return (block_t *)((char *)block + get_size(block));
}

/*
 * find_prev_footer: returns the footer of the previous block.
 */
static word_t *find_prev_footer(block_t *block) {
  // Compute previous footer position as one word before the header
  return &(block->header) - 1;
}

/*
 * find_prev: returns the previous block position by checking the previous
 *            block's footer and calculating the start of the previous block
 *            based on its size.
 */
static block_t *find_prev(block_t *block) {
  dbg_requires(block != NULL);
  dbg_requires(get_size(block) != 0);
  word_t *footerp = find_prev_footer(block);
  size_t size = extract_size(*footerp);
  return (block_t *)((char *)block - size);
}

/*
 * payload_to_header: given a payload pointer, returns a pointer to the
 *                    corresponding block.
 */
static block_t *payload_to_header(void *bp) {
  return (block_t *)((char *)bp - offsetof(block_t, payload));
}

/*
 * header_to_payload: given a block pointer, returns a pointer to the
 *                    corresponding payload.
 */
static void *header_to_payload(block_t *block) {
  return (void *)(block->payload);
}

/*
 * header_to_footer: given a block pointer, returns a pointer to the
 *                   corresponding footer.
 */
static word_t *header_to_footer(block_t *block) {
  return (word_t *)(block->payload + get_size(block) - dsize);
}
