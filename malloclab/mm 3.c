/*
 ******************************************************************************
 *                                   mm.c                                     *
 *           64-bit struct-based implicit free list memory allocator          *
 *                  15-213: Introduction to Computer Systems                  *
 *                                                                            *
 *  ************************************************************************  *
 *                  TODO: insert your documentation here. :)                  *
 *                                                                            *
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

// static const word_t prev_alloc_mask = 0x2;

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

// Pointer to first block
static block_t *heap_start = NULL;
static block_t *root = NULL;

/* Function prototypes for internal helper routines */

bool mm_checkheap(int lineno);

static block_t *extend_heap(size_t size);
static block_t *find_fit(size_t asize);
static block_t *coalesce_block(block_t *block);
static void split_block(block_t *block, size_t asize, size_t block_size);

static size_t max(size_t x, size_t y);
static size_t round_up(size_t size, size_t n);
static word_t pack(size_t size, bool alloc);

static size_t extract_size(word_t header);
static size_t get_size(block_t *block);
static size_t get_payload_size(block_t *block);

static bool extract_alloc(word_t header);
static bool get_alloc(block_t *block);

static void write_header(block_t *block, size_t size, bool alloc);
static void write_footer(block_t *block, size_t size, bool alloc);

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

/*
 * <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 */
bool mm_init(void) {
  // Create the initial empty heap
  word_t *start = (word_t *)(mem_sbrk(2 * wsize));

  if (start == (void *)-1) {
    return false;
  }

  /*
   * TODO: delete or replace this comment once you've thought about it.
   * Think about why we need a heap prologue and epilogue. Why do
   * they correspond to a block footer and header respectively?
   */
  start[0] = pack(0, true); // Heap prologue (block footer)
  start[1] = pack(0, true); // Heap epilogue (block header)

  // Heap starts with first "block header", currently the epilogue
  heap_start = (block_t *)&start[1];
  root = (block_t *)&start[1];

  // Extend the empty heap with a free block of chunksize bytes
  if (extend_heap(chunksize) == NULL) {
    return false;
  }

  return true;
}

/*
 * <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 */
void *malloc(size_t size) {
  //dbg_requires(mm_checkheap(__LINE__));

  size_t asize;      // Adjusted block size
  size_t extendsize; // Amount to extend heap if no fit is found
  block_t *block;
  void *bp = NULL;

  if (root == NULL) // Initialize heap if it isn't initialized
  {
    mm_init();
  }

  if (size == 0) // Ignore spurious request
  {
    dbg_ensures(mm_checkheap(__LINE__));
    return bp;
  }

  // Adjust block size to include overhead and to meet alignment requirements
  asize = round_up(size + dsize, dsize);

  // Search the free list for a fit
  block = find_fit(asize);
  printf("find block:%lx\n", (unsigned long)block);

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

  // The block should be marked as free
  dbg_assert(!get_alloc(block));

  // Mark block as allocated
  size_t block_size = get_size(block);
  write_header(block, asize, true);
  write_footer(block, asize, true);

  // Try to split the block if too large
  split_block(block, asize, block_size);

  bp = header_to_payload(block);

  dbg_ensures(mm_checkheap(__LINE__));
  return bp;
}

/*
 * <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
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

  // Mark the block as free
  write_header(block, size, false);
  write_footer(block, size, false);
  put(prev_linknode(bp), 0);
  put(next_linknode(bp), 0);



  // Try to coalesce the block with its neighbors
  printf("free block: %lx\n", (unsigned long)block);
  printf("free block size: %zu\n", get_size(block));
  block = coalesce_block(block);
  printf("free block after coalesce: %lx\n", (unsigned long)block);
  
  dbg_ensures(mm_checkheap(__LINE__));
}

/*
 * <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
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
 * <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
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
 * <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 */
static block_t *extend_heap(size_t size) {
  void *bp;

  // Allocate an even number of words to maintain alignment
  size = round_up(size, dsize);
  if ((bp = mem_sbrk(size)) == (void *)-1) {
    return NULL;
  }

  printf("extend heap bp: %lx\n", (unsigned long) bp);

  /*
   * TODO: delete or replace this comment once you've thought about it.
   * Think about what bp represents. Why do we write the new block
   * starting one word BEFORE bp, but with the same size that we
   * originally requested?
   */

  // Initialize free block header/footer
  block_t *block = payload_to_header(bp);

  printf("extend heap block: %lx\n", (unsigned long) block);
  
  write_header(block, size, false);
  write_footer(block, size, false);
  

  // Create new epilogue header
  block_t *block_next = find_next(block);
  write_header(block_next, 0, true);

  printf("extend heap block_next: %lx\n", (unsigned long) block_next);


  // Coalesce in case the previous block was free
  block = coalesce_block(block);

  printf("extend heap coalesce block: %lx\n", (unsigned long) block);


  return block;
}

/*
 * <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 */
static block_t *coalesce_block(block_t *block) {
  dbg_requires(!get_alloc(block));

  size_t size = get_size(block);

  /*
   * TODO: delete or replace this comment once you've thought about it.
   * Think about how we find the prev and next blocks. What information
   * do we need to have about the heap in order to do this? Why doesn't
   * "bool prev_alloc = get_alloc(block_prev)" work properly?
   */

  block_t *block_next = find_next(block);
  block_t *block_prev = find_prev(block);

  printf("coalesce prev block next: %lx, %lx, %lx\n", (unsigned long)block_next, (unsigned long)block, (unsigned long)block_prev);

  bool prev_alloc = extract_alloc(*find_prev_footer(block));
  bool next_alloc = get_alloc(block_next);

  printf("prev_alloc, next_alloc: %d, %d\n", prev_alloc, next_alloc);

  if (prev_alloc && next_alloc) // Case 1
  {
    if (root == block) {
      return block;
    }
    block_t *old_head = root;
    printf("coalesce old_head: %lx\n", (unsigned long)old_head);
    put(next_linknode(header_to_payload(block)), (word_t)old_head);
    printf("coalesce next, next_content: %lx, %lx\n", (unsigned long)next_linknode(header_to_payload(block)), (unsigned long) *((word_t *)next_linknode(header_to_payload(block))));
    put(prev_linknode(header_to_payload(block)), 0);
    root = block;
    if (old_head != NULL){
      put(prev_linknode(header_to_payload(old_head)), (word_t)block);
    }
  }

  else if (prev_alloc && !next_alloc) // Case 2
  {
    size += get_size(block_next);
    block_t *old_next = get(next_linknode(header_to_payload(block_next)));
    block_t *old_prev = get(prev_linknode(header_to_payload(block_next)));
    if (old_prev != NULL) {    //next block is not the first free block
      block_t *old_head = root;
      put(next_linknode(header_to_payload(block)), (word_t) old_head);
      put(prev_linknode(header_to_payload(block)), 0);
      root = block;
      if (old_head != NULL) {   //need validation maybe
        put(prev_linknode(header_to_payload(old_head)), (word_t)block);
      }
      if (old_next != NULL) {
        put(prev_linknode(header_to_payload(old_next)), (word_t) old_prev);
      }
      put(next_linknode(header_to_payload(old_prev)), (word_t) old_next);
    } else {    
      if (root == block_next) {
        root = block;
        put(prev_linknode(header_to_payload(block)), 0);
        put(next_linknode(header_to_payload(block)), (word_t) old_next);
        if (old_next != NULL) {
          put(prev_linknode(header_to_payload(old_next)), (word_t) block);
        }
      } else {
        printf("next block is free and have a null prev pointer, but root is not pointed to it.\n");
        return block;
      }
    }

    write_header(block, size, false);
    write_footer(block, size, false);
  }

  else if (!prev_alloc && next_alloc) // Case 3
  {
    size += get_size(block_prev);
    block_t *old_next = get(next_linknode(header_to_payload(block_prev)));
    block_t *old_prev = get(prev_linknode(header_to_payload(block_prev)));
    if (old_prev != NULL) {
      block_t *old_head = root;
      put(next_linknode(header_to_payload(block_prev)), (word_t) old_head);
      put(prev_linknode(header_to_payload(block_prev)), 0);
      root = block_prev;
      if (old_head != NULL) {   //need validation maybe
        put(prev_linknode(header_to_payload(old_head)), (word_t)block_prev);
      }
      if (old_next != NULL) {
        put(prev_linknode(header_to_payload(old_next)), (word_t)old_prev);
      }
      put(next_linknode(header_to_payload(old_prev)), (word_t) old_next);
    } else {
      if (root != block_prev) {
        printf("prev block is free and have a null prev pointer, but root is not pointed to it.\n");
        return block;
      }
    }
    write_header(block_prev, size, false);
    write_footer(block_prev, size, false);
    block = block_prev;
  }

  else // Case 4
  {
    size += get_size(block_next) + get_size(block_prev);
    block_t *prev_prev = get(prev_linknode(header_to_payload(block_prev)));
    block_t *prev_next = get(next_linknode(header_to_payload(block_prev)));
    block_t *next_prev = get(prev_linknode(header_to_payload(block_next)));
    block_t *next_next = get(next_linknode(header_to_payload(block_next)));
    printf("prev_prev, prev_next, next_prev, next_next: %d, %d, %d, %d\n", (prev_prev==NULL), (prev_next==NULL), (next_prev==NULL), (next_next==NULL));
    if (prev_prev != NULL && next_prev != NULL) {
      block_t *old_head = root;
      if (prev_next == block_next) {
        put(next_linknode(header_to_payload(block_prev)), (word_t)next_next);
        if (next_next != NULL) {
          put(prev_linknode(header_to_payload(next_next)), (word_t)block_prev);
        }
        
      } else if (next_next == block_prev) {
        put(prev_linknode(header_to_payload(block_prev)), (word_t)next_prev);
        put(next_linknode(header_to_payload(next_prev)), (word_t)block_prev);
      } else {
        put(next_linknode(header_to_payload(block_prev)), (word_t) old_head);
        put(prev_linknode(header_to_payload(block_prev)), 0);
        root = block_prev;
        if (old_head != NULL) {   //need validation maybe
          put(prev_linknode(header_to_payload(old_head)), (word_t)block_prev);
        }


        if (prev_next != NULL) {
          put(prev_linknode(header_to_payload(prev_next)), (word_t)prev_prev);
        }
        put(next_linknode(header_to_payload(prev_prev)), (word_t) prev_next);


        if (next_next != NULL) {
          put(prev_linknode(header_to_payload(next_next)), (word_t)next_prev);
        }
        put(next_linknode(header_to_payload(next_prev)), (word_t) next_next);
      }
      
    } 

    if (prev_prev == NULL && next_prev != NULL) {
      if (root == block_prev) {
        
        if (prev_next == block_next) {
          put(next_linknode(header_to_payload(block_prev)), (word_t)next_next);
          if (next_next != NULL) {
            put(prev_linknode(header_to_payload(next_next)), (word_t)block_prev);
          }
          put(prev_linknode(header_to_payload(block_next)), 0);
          put(next_linknode(header_to_payload(block_next)), 0);
        } else {
          if (next_next != NULL) {
            put(prev_linknode(header_to_payload(next_next)), (word_t)next_prev);
          }
          put(next_linknode(header_to_payload(next_prev)), (word_t) next_next);
        }

      
      } else {
        printf("prev block is free and have a null prev pointer, but root is not pointed to it.\n");
        printf("The block is %lx\n", (unsigned long)block);
        return block;
      }
    }

    if (prev_prev != NULL && next_prev == NULL) {
      // printf("1\n");
      if (root == block_next) {
        // printf("1\n");
        root = block_prev;
        if (next_next == block_prev) {
          put(prev_linknode(header_to_payload(block_prev)), 0);
        } else {
          put(prev_linknode(header_to_payload(block_prev)), 0);
          put(next_linknode(header_to_payload(block_prev)), (word_t) next_next);
          if (next_next != NULL) {
            put(prev_linknode(header_to_payload(next_next)), (word_t) block_prev);
          }

          if (prev_next != NULL) {
            put(prev_linknode(header_to_payload(prev_next)), (word_t)prev_prev);
          }
          put(next_linknode(header_to_payload(prev_prev)), (word_t) prev_next);
        }
        
      
      } else {
        printf("next block %lx is free and have a null prev pointer, but root is not pointed to it.\n", (unsigned long)block);

        return block;
      }

    }
    if (prev_prev == NULL && next_prev == NULL) {
      printf("both the prev and next blocks have a null prev pointer.\n");
      printf("The block is %lx\n", (unsigned long)block);
      return block;
    }
    write_header(block_prev, size, false);
    write_footer(block_prev, size, false);
    block = block_prev;
  }

  dbg_ensures(!get_alloc(block));

  return block;
}

/*
 * <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 */
static void split_block(block_t *block, size_t asize, size_t block_size) {
  dbg_requires(get_alloc(block));
  /* TODO: Can you write a precondition about the value of asize? */

  

  if ((block_size - asize) >= min_block_size) {
    block_t *block_next;
    write_header(block, asize, true);
    write_footer(block, asize, true);
    block_t *prev = get(prev_linknode(header_to_payload(block)));
    block_t *next = get(next_linknode(header_to_payload(block)));

    block_next = find_next(block);
    printf("split block, block_next: %lx, %lx\n", (unsigned long)block, (unsigned long)block_next);
    write_header(block_next, block_size - asize, false);
    put(prev_linknode(header_to_payload(block_next)), (uint64_t)prev);
    put(next_linknode(header_to_payload(block_next)), (uint64_t)next);
    if (root == block) {
      root = block_next;
    }
    
    write_footer(block_next, block_size - asize, false);
    if (prev != NULL) {
      put(next_linknode(header_to_payload(prev)), (uint64_t)block_next);
    }
    if (next != NULL) {
      put(prev_linknode(header_to_payload(next)), (uint64_t)block_next);
    }  
  } else {
    write_header(block, block_size, true);
    write_footer(block, block_size, true);
    block_t *prev = get(prev_linknode(header_to_payload(block)));
    block_t *next = get(next_linknode(header_to_payload(block)));
    if (prev != NULL) {
      if (next != NULL) {
        put(prev_linknode(header_to_payload(next)), (word_t)prev);

      }
      put(next_linknode(header_to_payload(prev)), (word_t)next);
    } else {
      if (root == block) {
        root = get(next_linknode(header_to_payload(root)));
        if (root != NULL) {
          put(prev_linknode(header_to_payload(root)), 0);
        }
        
      } else {
        printf("block is free and have a null prev pointer, but root is not pointed to it.\n");
      }
    }
    
  }

  dbg_ensures(get_alloc(block));
  
  printf("split root: %lx\n", (unsigned long)root);
}

/*
 * <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 */
static block_t *find_fit(size_t asize) {
  block_t *block;
  block = root;

  while (block != NULL) {

    if (!(get_alloc(block)) && (asize <= get_size(block))) {
      return block;
    }

    block = get(next_linknode(header_to_payload(block)));
  }
  return NULL; // no fit found
}

static void *prev_linknode(void *bp) {
  return bp;
}

static void *next_linknode(void *bp) {
  return (void *)((unsigned char *)bp + wsize);
}

static void put(void *p, uint64_t val) {
  *((word_t *) p) = val;
}

static block_t *get(void *p) {
  return (block_t *) (*(uint64_t *)(p));
}

/*
 * <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 */
bool mm_checkheap(int line) {
  /*
   * TODO: Delete this comment!
   *
   * You will need to write the heap checker yourself.
   * Please keep modularity in mind when you're writing the heap checker!
   *
   * As a filler: one guacamole is equal to 6.02214086 x 10**23 guacas.
   * One might even call it...  the avocado's number.
   *
   * Internal use only: If you mix guacamole on your bibimbap,
   * do you eat it with a pair of chopsticks, or with a spoon?
   */
  // check free lists
  // 1. if the prev-next == this.prev ...
  // 2. if root.prev == NULL
  // 3. if the next prev of the free block are both allocated.
  // 4. if the header == footer
  block_t *block;
  if (root != NULL) {
    if (get(prev_linknode(header_to_payload(root))) != 0) {
      printf("prev: %lx, next: %lx\n", (unsigned long)prev_linknode(header_to_payload(root)), (unsigned long)next_linknode(header_to_payload(root)));
      printf("get_prev: %lx\n", (unsigned long)get(prev_linknode(header_to_payload(root))));
      printf("%d: the prev pointer of root block is not NULL.\n", line);
      return false;
    }
    for (block = root; block != NULL; block = get(next_linknode(header_to_payload(block)))) {
      word_t block_header = block->header;
      word_t block_footer = *(header_to_footer(block));
      printf("header: %lu, footer: %lu\n", (unsigned long)block_header, (unsigned long)block_footer);
      unsigned long diff = (unsigned long)block_header - (unsigned long)block_footer;
      printf("diff: %lu\n", diff);
      // if (diff != 0L) {
      //   printf("%d: block header not equal to footer.\n", line);
      //   return false;
      // }


      block_t *next = get(next_linknode(header_to_payload(block)));
      block_t *prev = get(prev_linknode(header_to_payload(block)));
      
      if (block != root) {
        if (prev == NULL) {
          printf("%d: not the root block has a NULL prev pointer.\n", line);
          printf("The block is %lx\n", (unsigned long)block);
          return false;
        }
      }

      if (next != NULL) {
        block_t *next_prev = get(prev_linknode(header_to_payload(next)));
        if (next_prev != block) {
          printf("%d: the prev pointer of the next block does not point to this block.\n", line);
          printf("The block is %lx\n", (unsigned long)block);
          return false;
        }
      }

      if (prev != NULL) {
        block_t *prev_next = get(next_linknode(header_to_payload(prev)));
        if (prev_next != block) {
          printf("%d: the next pointer of the prev block does not point to this block.\n", line);
          printf("The block is %lx\n", (unsigned long)block);
          return false;
        }
      }

      block_t *block_next = find_next(block);
      block_t *block_prev = find_prev(block);
      if (block_next != NULL) {
        word_t block_header = block_next->header;
        word_t block_footer = *(header_to_footer(block_next));
        unsigned long diff = (unsigned long)block_header - (unsigned long)block_footer;
        printf("header: %lu, footer: %lu\n", (unsigned long)block_header, (unsigned long)block_footer);
        printf("diff: %lu\n", diff);
        // if (diff != 0L) {
        //   printf("%d: block header not equal to footer.\n", line);
        //   return false;
        // }
        
      }

        if (!get_alloc(block_next)) {
          printf("%d: the next block of the free block is also free.\n", line);
          printf("The block is %lx\n", (unsigned long)block);
          return false;
        }
      //   if (get_size(block_next) != 0) {
      //     block_t *block_next_next = find_next(block_next);
      //     if (block_next_next != NULL) {
      //       if (get_alloc(block_next_next)) {
      //         printf("%d: the next block of an allocated block is also allocated.\n", line);
      //         return false;
      //       }
      //     }
      //   }
      // }
      if (block_prev != NULL) {
        word_t block_header = block_prev->header;
        word_t block_footer = *(header_to_footer(block_prev));
        if (block_header != block_footer) {
          printf("%d: block header not equal to footer.\n", line);
          printf("The block is %lx\n", (unsigned long)block);
          return false;
        }

        if (block != block_prev && !get_alloc(block_prev)) {
          printf("%d: the prev block of the free block is also free.\n", line);
          printf("The block is %lx\n", (unsigned long)block);
          return false;
        }
      //   if (get_size(block_prev) != 0) {
      //     block_t *block_prev_prev = find_prev(block_prev);
      //     if (block_prev_prev != NULL) {
      //       if (get_alloc(block_prev_prev)) {
      //         printf("%d: the prev block of an allocated block is also allocated.\n", line);
      //         return false;
      //       }
      //     }
      //   }
      // }

      }
    }
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
static word_t pack(size_t size, bool alloc) {
  // word_t p = 0;
  // if (alloc) {
  //   if (prev) {
  //     return (size | alloc_mask | prev_alloc_mask);
  //   } else {
  //     return (size | alloc_mask);
  //   }
  // } else {
  //   if (prev) {
  //     return (size | prev_alloc_mask);
  //   } else {
  //     return size;
  //   }
  // }
  return alloc? (size | alloc) : size;
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
  return asize - dsize;
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
 * write_header: given a block and its size and allocation status,
 *               writes an appropriate value to the block header.
 * TODO: Are there any preconditions or postconditions?
 */
static void write_header(block_t *block, size_t size, bool alloc) {
  dbg_requires(block != NULL);
  block->header = pack(size, alloc);
}

/*
 * write_footer: given a block and its size and allocation status,
 *               writes an appropriate value to the block footer by first
 *               computing the position of the footer.
 * TODO: Are there any preconditions or postconditions?
 */
static void write_footer(block_t *block, size_t size, bool alloc) {
  dbg_requires(block != NULL);
  dbg_requires(get_size(block) == size && size > 0);
  word_t *footerp = header_to_footer(block);
  *footerp = pack(size, alloc);
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
