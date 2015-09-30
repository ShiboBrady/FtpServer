CC = gcc

CFLAGS = -Wall -g

VPATH = :src

LIB = -lcrypt

EXE_DIR = bin

OBJ_DIR = obj

SRC_DIR = src

INCLUDE_DIR = include

BIN = $(EXE_DIR)/FtpServer

OBJS = $(subst .c,.o,$(subst $(SRC_DIR),$(OBJ_DIR),$(wildcard $(SRC_DIR)/*.c)))

$(shell mkdir -p $(OBJ_DIR) $(EXE_DIR))

.PHONY : clean

$(BIN):$(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LIB)

$(OBJ_DIR)/%.o:%.c
	$(CC) $(CFLAGS) -c $< -o $@ -I$(INCLUDE_DIR)

clean:
	rm -rf $(OBJ_DIR) $(EXE_DIR)
