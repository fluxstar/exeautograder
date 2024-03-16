# Variables
CC=gcc
CFLAGS=-Wall -g

SRCDIR=src
INCDIR=include
LIBDIR=lib

SOL_DIR=solutions

SOURCE_FILE=$(SRCDIR)/template.c
N ?= 8
BINARIES=$(addprefix $(SOL_DIR)/sol_, $(shell seq 1 $(N)))

# Default target
auto: autograder $(BINARIES)

mq_auto: mq_autograder worker $(BINARIES)

# Compile autograder
autograder: $(SRCDIR)/autograder.c $(LIBDIR)/utils.o
	$(CC) $(CFLAGS) -I$(INCDIR) -o $@ $< $(LIBDIR)/utils.o

# Compile mq_autograder
mq_autograder: $(SRCDIR)/mq_autograder.c $(LIBDIR)/utils.o
	$(CC) $(CFLAGS) -I$(INCDIR) -o $@ $< $(LIBDIR)/utils.o

# Compile worker
worker: $(SRCDIR)/worker.c $(LIBDIR)/utils.o
	$(CC) $(CFLAGS) -I$(INCDIR) -o $@ $< $(LIBDIR)/utils.o

# Compile utils.c into utils.o
$(LIBDIR)/utils.o: $(SRCDIR)/utils.c
	$(CC) $(CFLAGS) -I$(INCDIR) -c -o $@ $< 

# Compile worker.c into worker.o
$(LIBDIR)/worker.o: $(SRCDIR)/worker.c
	$(CC) $(CFLAGS) -I$(INCDIR) -c -o $@ $<

# Compile template.c into N binaries
$(SOL_DIR)/sol_%: $(SOURCE_FILE)
	mkdir -p $(SOL_DIR)
	$(CC) $(CFLAGS) -o $@ $<

# Cases
exec: CFLAGS += -DEXEC
exec: auto

redir: CFLAGS += -DREDIR
redir: auto

pipe: CFLAGS += -DPIPE
pipe: auto

mqueue: CFLAGS += -DMQUEUE
mqueue: mq_auto

# Test case 1: "make test1_exec N=8"
test1_exec: exec
	./autograder solutions 1 2 3

# Clean the build
clean:
	rm -f autograder mq_autograder worker
	rm -f solutions/sol_*
	rm -f $(LIBDIR)/*.o
	rm -f input/*.in output/*

.PHONY: auto clean exec redir pipe mqueue