CC=gcc
LIBS=-lncurses

SRC_DIR=.
BIN_DIR=./bin
BINS = ${BIN_DIR}/client

${BIN_DIR}/%: %.o
	${CC} -o $@.out $< ${LIBS}

%.o: 	${SRC_DIR}/%.c
	${CC} -o $@ -c $<
all:	${BINS}
clean: @rm -f ${BINS}
