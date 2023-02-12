CC=gcc
IDIR=../include
CFLAGS=-I../build/include -I$(IDIR) -g -Wall

ODIR=obj
LDIR=../build/lib

LIBS=-lfabric -L../build/lib

_DEPS=bk_libfab.h
DEPS=$(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ =  bk_libfab_coll.o bk_util.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

BDIR=bin
_BIN=bk_libfab_coll
BIN = $(patsubst %,$(BDIR)/%,$(_BIN))

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(BIN): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)
	
.PHONY: test clean
	
clean: 
	rm $(ODIR)/*.o $(BDIR)/*
