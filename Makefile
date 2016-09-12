CC=gcc
LIBS=-lncurses

SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)
BIN_DIR=./bin
BINS = ${BIN_DIR}/client

${BIN_DIR}/%: $(OBJ)
	$(CC) -o $@.out $(OBJ) $(LIBS)

%.o: 	%.c
	$(CC) -o $@ -c $< -I./include 
all:	$(BINS)
clean: @rm -f $(BINS)
