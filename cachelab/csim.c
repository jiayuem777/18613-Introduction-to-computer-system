/* Name: Jiayue Mao */
/* Andrew ID: jiayuem */

#include "cachelab.h"
#include <stdio.h>  /* fopen freopen perror */
#include <stdint.h> /* uintN_t */
#include <unistd.h> /* getopt */
#include <getopt.h> /* getopt -std=c99 POSIX macros defined in <features.h> prevents <unistd.h> from including <getopt.h>*/
#include <stdlib.h> /* atol exit*/
#include <errno.h>  /* errno */

#define true 1
#define false 0

typedef struct {
    _Bool valid;
    _Bool dirty;
    uint64_t tag;
    uint64_t counter;
} line;

typedef line *set;
typedef set *cache;

typedef struct {
    int test;
    int hits;
    int misses;
    int evictions;
    int dirty_bytes_in_cache;
    int dirty_bytes_evicted;
} result;

void print_massage_out_of_memory() {
    printf("Set out of memory.\n");
    exit(EXIT_FAILURE);
}

void print_massage_file_not_open() {
    printf("Trace file cannot open.\n");
    exit(EXIT_FAILURE);
}

void print_massage_malloc_fail() {
    printf("malloc failed.\n");
    exit(EXIT_FAILURE);
}

cache MakeCache(unsigned E, unsigned set_bit, unsigned block_bit) {
    unsigned S = 1 << set_bit;

    cache cache = calloc(S, sizeof(set));
    if (cache == NULL) {
        print_massage_malloc_fail();
    }
    int i, j;
    for (i = 0; i < S; i++) {
        cache[i] = calloc(E, sizeof(line));
        if (cache[i] == NULL) {
            print_massage_malloc_fail();
        }
        for (j = 0; j < E; j++) {
            cache[i][j].valid = false;
            cache[i][j].dirty = false;
            cache[i][j].tag = 0;
            cache[i][j].counter = UINT64_MAX;
        }
    }
    return cache;
}

void FreeCache(cache cache, unsigned S, unsigned E) {
    int i;
    for (i = 0; i < S; i++) {
        free(cache[i]);
    }
    free(cache);
}

result TestCache(result result, uint64_t tag, unsigned E, unsigned B, set set, _Bool verbose, char op) {
    int i = 0;
    _Bool hit_flag = false;
    long youngest_time = (long) set[0].counter;
    for (i = 0; i < E; i++) {
        if (youngest_time < set[i].counter) {
            youngest_time = set[i].counter;
        }
    }
    //hit
    for (i = 0; i < E; i++) {
        if (set[i].valid && set[i].tag == tag) {
            result.hits = result.hits + 1;
            hit_flag = true;
            if (op == 'S') {
                if (!set[i].dirty) {
                    result.dirty_bytes_in_cache += B;
                    set[i].dirty = true;
                }
            } 
            set[i].counter = youngest_time + 1;
            if (verbose) {
                printf("hit\n");
            }
        }
    }

    //miss
    if (!hit_flag) {
        result.misses += 1;
        if (verbose) {
            printf("miss");
        }

        long oldest_time = (long) set[0].counter;
        long youngest_time = (long) set[0].counter;
        int oldest_index = 0;
        for (i = 0; i < E; i ++) {
            if (oldest_time > (long) set[i].counter) {
                oldest_time = (long) set[i].counter;
                oldest_index = i;
            }
            if (youngest_time < (long) set[i].counter) {
                youngest_time = (long) set[i].counter;
            }
        }
        if (oldest_time < 0) {
            if (verbose) {
                printf("\n");
            }
            set[oldest_index].tag = tag;
            set[oldest_index].valid = true;
            set[oldest_index].dirty = false;
            set[oldest_index].counter = youngest_time + 1;
            if (op == 'S') {
                result.dirty_bytes_in_cache += B;
                set[oldest_index].dirty = true;
            }
        } else {
            if (verbose) {
                printf(" eviction\n");
            }
            result.evictions += 1;
            set[oldest_index].tag = tag;
            set[oldest_index].counter = youngest_time + 1;
            if (op == 'S') {
                if (set[oldest_index].dirty) {
                    result.dirty_bytes_evicted += B;
                } else {
                    result.dirty_bytes_in_cache += B;
                    set[oldest_index].dirty = true;
                }
            }
            if (op == 'L') {
                if (set[oldest_index].dirty) {
                    result.dirty_bytes_evicted += B;
                    result.dirty_bytes_in_cache -= B;
                    set[oldest_index].dirty = false;
                }
            }         
        }
    }
    
    return result;
}

result ReadTraceFile(cache cache, FILE *tracefile, unsigned S, unsigned set_bit,
unsigned E, unsigned block_bit, _Bool verbose) {
    result result = {0, 0, 0, 0, 0, 0};
    unsigned B = 1 << block_bit;
    char op;
    uint64_t addr;
    int size;
    while(fscanf(tracefile, " %s %lx,%d", &op, &addr, &size) == 3) {
        uint64_t set_mask = (1L << set_bit) - 1L;
        uint64_t set_index = (addr >> block_bit) & set_mask;
        uint64_t tag = addr >> (block_bit + set_bit);
        if (set_index < S) {
            set set = cache[set_index];
            if (verbose) {
            printf("%c %lx ", op, addr);
        }
        result = TestCache(result, tag, E, B, set, verbose, op);
        } else {
            print_massage_out_of_memory();
        }
    }
    return result;
}

int main(int argc, char *argv[]) {
    FILE* tracefile = NULL;
    cache cache = NULL;
    _Bool verbose;
    unsigned E; /* number of lines per set */
    unsigned set_bit;
    unsigned block_bit;
    const char *massage = "s, b should not be negative and E should be above 0.";

    char co; /* command options */
    while((co = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
        switch (co) {
            case 'h':
                printf("%s\n", massage);
                break;
            case 'v':
                verbose = true;
                break;
            case 's':
                set_bit = atoi(optarg);
                if (set_bit < 0) {
                    printf("%s\n", massage);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'E':
                E = atoi(optarg);
                if (E <= 0) {
                    printf("%s\n", massage);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'b':
                block_bit = atoi(optarg);
                if (block_bit < 0) {
                    printf("%s\n", massage);
                    exit(EXIT_FAILURE);
                }
                break;
            case 't':
                tracefile = fopen(optarg, "r");
                if (tracefile == NULL) {
                    print_massage_file_not_open();
                }
                break;
            default:
                printf("%s\n", massage);
                exit(EXIT_FAILURE);
        }
    }
    unsigned S = 1 << set_bit;
    cache = MakeCache(E, set_bit, block_bit);
    result result = ReadTraceFile(cache, tracefile, S, set_bit, E, block_bit, verbose);
    FreeCache(cache, S, E);
    printSummary(result.hits, result.misses, result.evictions, result.dirty_bytes_in_cache, result.dirty_bytes_evicted);
    return 0;
}
