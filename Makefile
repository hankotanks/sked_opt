CC := gcc
OUT := opt

all: $(OUT)

DIR_INC := include
DIR_SRC := src
DIR_OBJ := build

CFLAGS := -std=c99 -Wall -Wno-unused-value -Wextra -Wconversion -Wpedantic -I$(DIR_INC) -DHH_LOG=HH_LOG_DBG -DROOT=\"$(dir $(abspath $(lastword $(MAKEFILE_LIST))))\"
	
$(OUT): $(patsubst $(DIR_SRC)/%.c, $(DIR_OBJ)/%.o, $(wildcard $(DIR_SRC)/*.c))
# build target executable
	$(CC) $(CFLAGS) $^ -o $@

$(DIR_OBJ)/%.o: $(DIR_SRC)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) -r $(DIR_OBJ)/*.o

.PHONY: all clean