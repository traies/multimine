CC=gcc  -std=c99
LIBS=-lncurses -pthread

COMMON_SRC= queue.c commsS_TCP.c configurator.c
COMMON_OBJ= $(COMMON_SRC:.c=.o)
CLIENT_SRC= client.c
CLIENT_OBJ= $(CLIENT_SRC:.c=.o)
SERVER_SRC= server.c
SERVER_OBJ= $(SERVER_SRC:.c=.o)
MQ_SRC= mq.c
MQ_OBJ= $(MQ_SRC:.c=.o)
BIN_CLIENT=./bin/client.out
BIN_SERVER=./bin/server.out
BIN_MQ=./bin/mq.out

all:	$(BIN_CLIENT) $(BIN_SERVER) $(BIN_MQ)

.c.o:
	$(CC) -c $< -o $@ -I./include


$(BIN_CLIENT): $(COMMON_OBJ) $(CLIENT_OBJ)
	$(CC) -o $(BIN_CLIENT) $(CLIENT_OBJ) $(COMMON_OBJ) $(LIBS)

$(BIN_MQ): $(COMMON_OBJ) $(MQ_OBJ)
		$(CC) -o $(BIN_MQ) $(MQ_OBJ) $(COMMON_OBJ) $(LIBS) -lrt

$(BIN_SERVER): $(COMMON_OBJ) $(SERVER_OBJ)
	$(CC) -o $(BIN_SERVER) $(SERVER_OBJ) $(COMMON_OBJ) $(LIBS) -lrt





clean:
	rm -f $(BIN_CLIENT) $(BIN_SERVER)  $(BIN_MQ) *.o
