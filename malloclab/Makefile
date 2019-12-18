#
# Makefile for the malloc lab driver
#
# C Compiler
CLANG = clang
LLVM_PATH = /usr/local/depot/llvm-7.0/bin/
CC = $(LLVM_PATH)$(CLANG)

# Additional flags used to compile mdriver-dbg
# You can edit these freely to change how your debug binary compiles.
COPT_DBG = -Og
CFLAGS_DBG = -DDEBUG=1

# Flags used to compile normally
COPT = -O3
CFLAGS = -Wall -Wextra -Werror $(COPT) -g -DDRIVER -Wno-unused-function -Wno-unused-parameter

# Build configuration
FILES = mdriver mdriver-dbg mdriver-emulate
LDLIBS = -lm -lrt
COBJS = memlib.o fcyc.o clock.o stree.o
MDRIVER_HEADERS = fcyc.h clock.h memlib.h config.h mm.h stree.h

MC = ./macro-check.pl
MCHECK = $(MC) -i dbg_

# Default rule
all: $(FILES)
	-clang-format -style=llvm -i mm.c

# Regular driver
mdriver: mdriver.o mm-native.o $(COBJS)
	$(CC) -o $@ $^ $(LDLIBS)

# Debug driver
mdriver-dbg: COPT = $(COPT_DBG)
mdriver-dbg: CFLAGS += $(CFLAGS_DBG)
mdriver-dbg: mdriver.o mm-native-dbg.o $(COBJS)
	$(CC) -o $@ $^ $(LDLIBS)

# Sparse-mode driver for checking 64-bit capability
mdriver-emulate: mdriver-sparse.o mm-emulate.o $(COBJS)
	$(CC) -o $@ $^ $(LDLIBS)

# Version of memory manager with memory references converted to function calls
mm-emulate.o: mm.c mm.h memlib.h MLabInst.so
	$(LLVM_PATH)$(CLANG) $(CFLAGS) -fno-vectorize -emit-llvm -S mm.c -o mm.bc
	$(LLVM_PATH)opt -load=./MLabInst.so -MLabInst mm.bc -o mm_ct.bc
	$(LLVM_PATH)$(CLANG) -c $(CFLAGS) -o mm-emulate.o mm_ct.bc

mm-native.o: mm.c mm.h memlib.h $(MC)
	$(MCHECK) -f $<
	$(LLVM_PATH)$(CLANG) $(CFLAGS) -c -o $@ $<

mm-native-dbg.o: mm.c mm.h memlib.h $(MC)
	$(LLVM_PATH)$(CLANG) $(CFLAGS) -c -o $@ $<

mdriver-sparse.o: mdriver.c $(MDRIVER_HEADERS)
	$(CC) -g $(CFLAGS) -DSPARSE_MODE -c mdriver.c -o mdriver-sparse.o

mdriver.o: mdriver.c $(MDRIVER_HEADERS)
memlib.o: memlib.c memlib.h
mm.o: mm.c mm.h memlib.h
fcyc.o: fcyc.c fcyc.h
ftimer.o: ftimer.c ftimer.h config.h
clock.o: clock.c clock.h
stree.o: stree.c stree.h

clean:
	rm -f *~ *.o *.bc *.ll
	rm -f $(FILES)

.PHONY: all clean
