CUDDDIR = cudd-3.0.0
CFLAGS=-O2 

all:
	(cd $(CUDDDIR); make ; make install)
	(cd cbit; make all)

clean:
	rm -f *~
	(cd $(CUDDDIR); make clean)
	(cd cbit; make clean)


