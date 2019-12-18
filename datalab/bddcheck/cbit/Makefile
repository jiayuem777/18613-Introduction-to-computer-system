CC=gcc
CFLAGS=-Wall -g -O2
LCFLAGS=-g -O2
LEX = flex
YACC=bison
LEXLIB = -lfl
CUDDDIR = ../cudd-3.0.0
CUDDINC = -I$(CUDDDIR)/include
INC = $(CUDDINC)
CUDDLIBS = $(CUDDDIR)/lib/libcudd.a -lm

all: cbit

cbit: boolnet.h ast.h gen-hash.h mem.h lex.yy.o simplec.tab.o ast-gen.c boolnet.c ast-eval.c gen-hash.c mem.c
	$(CC) $(CFLAGS) $(INC) lex.yy.o simplec.tab.o ast-gen.c boolnet.c ast-eval.c gen-hash.c mem.c $(CUDDLIBS) -o cbit

cbit-cnf: boolnet.h ast.h gen-hash.h mem.h lex.yy.o simplec.tab.o ast-gen.c boolnet.c ast-eval.c gen-hash.c mem.c
	$(CC) $(CFLAGS) -DCNFONLY lex.yy.o simplec.tab.o ast-gen.c boolnet.c ast-eval.c gen-hash.c mem.c -o cbit-cnf


lex.yy.c: simplec.lex
	$(LEX) simplec.lex

lex.yy.o: lex.yy.c boolnet.h ast.h simplec.tab.c gen-hash.h
	$(CC) $(LCFLAGS) -c $(INC) lex.yy.c

simplec.tab.o: simplec.tab.c boolnet.h ast.h
	$(CC) $(LCFLAGS) -c $(INC) simplec.tab.c

simplec.tab.c: simplec.y
	$(YACC) -d simplec.y

fcheck: fcheck.c boolnet.h boolnet.c gen-hash.h gen-hash.c
	$(CC) $(CFLAGS) $(INC) fcheck.c boolnet.c gen-hash.c $(CUDDLIBS) -o fcheck

float-mul: float-mul.c boolnet.h boolnet.c gen-hash.h gen-hash.c
	$(CC) $(CFLAGS) $(INC) float-mul.c boolnet.c gen-hash.c $(CUDDLIBS) -o float-mul

clean:
	rm -f *.o *.exe cbit simplec.tab.c simplec.tab.h lex.yy.c *~


