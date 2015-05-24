PVMINC=$HOME/pvm3/pvm3/include
PVMLIB=$HOME/pvm3/pvm3/lib/$(PVM_ARCH) 

all:	$(PVM_HOME)/starter $(PVM_HOME)/skier

run:
	$(PVM_HOME)/starter

$(PVM_HOME)/starter:	starter.c def.h
	cc -g starter.c -o $(PVM_HOME)/starter -L$(PVMLIB) -I$(PVMINC) -lpvm3 -lgpvm3

$(PVM_HOME)/skier:	skier.c def.h
	cc -g skier.c -o $(PVM_HOME)/skier -L$(PVMLIB) -I$(PVMINC) -lpvm3 -lgpvm3

clean:
	rm $(PVM_HOME)/starter $(PVM_HOME)/skier
	rm -f *.o

