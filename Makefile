CC=gcc
CFLAGS= -g3 -pthread
SRC_DIR=src
OBJ_DIR=obj

SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

.PHONY: all clean

all: dataServer remoteClient

dataServer: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ_DIR)/list.o $(OBJ_DIR)/list_type.o $(OBJ_DIR)/dataServer.o

remoteClient: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ_DIR)/remoteClient.o

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
		$(CC) $(CFLAGS) -c -o $@ $<

clean:
	$(RM) $(OBJ) dataServer remoteClient
