CC=gcc
LIBS=-lncurses

COMMON_SRC= queue.c comms.c marsh.c
COMMON_OBJ= $(COMMON_SRC:.c=.o) 
CLIENT_SRC= client.c
CLIENT_OBJ= $(CLIENT_SRC:.c=.o)
SERVER_SRC= server.c
SERVER_OBJ= $(SERVER_SRC:.c=.o)
BIN_CLIENT=./bin/client.out
BIN_SERVER=./bin/server.out


all:	$(BIN_CLIENT) $(BIN_SERVER)

.c.o:
	$(CC) -c $< -o $@ -I./include


$(BIN_CLIENT): $(COMMON_OBJ) $(CLIENT_OBJ)
	$(CC) -o $(BIN_CLIENT) $(CLIENT_OBJ) $(COMMON_OBJ) $(LIBS)

$(BIN_SERVER): $(COMMON_OBJ) $(SERVER_OBJ)
	$(CC) -o $(BIN_SERVER) $(SERVER_OBJ) $(COMMON_OBJ) $(LIBS)



clean: 
	rm -f $(BIN_CLIENT) $(BIN_SERVER) *.o
