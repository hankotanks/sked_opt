CC := gcc
OUT := opt

all: $(OUT)

DIR_INC := include
DIR_SRC := src
DIR_OBJ := build

CFLAGS := -ggdb3 -std=c99 -Wall -Wno-implicit-fallthrough -Wno-unused-value -Wno-override-init -Wno-override-init-side-effects -Wextra -Wconversion -Wpedantic -I$(DIR_INC) -isystem$(GUROBI_HOME)/include -isystemsofa -DHH_LOG=HH_LOG_DBG -DPROJECT_ROOT=\"$(dir $(abspath $(lastword $(MAKEFILE_LIST))))\"
LDLIBS := -lm -lsofa_c
LDFLAGS := -Lsofa

ifneq ($(OS),Windows_NT)
    LDLIBS += -ldl
endif

$(OUT): $(patsubst $(DIR_SRC)/%.c, $(DIR_OBJ)/%.o, $(wildcard $(DIR_SRC)/*.c))
# build dependencies
	$(MAKE) -s -C glenv
	$(MAKE) -s -C sofa
# build target executable
	$(CC) $(CFLAGS) $^ -o $@ $(shell $(MAKE) get_bin_flags -s -C glenv) $(LDFLAGS) $(LDLIBS)

$(DIR_OBJ)/%.o: $(DIR_SRC)/%.c
	$(CC) $(CFLAGS) -c $< -o $@ $(shell $(MAKE) get_obj_flags -s -C glenv)

clean-full: clean
	$(MAKE) clean -s -C glenv
	$(MAKE) clean -s -C sofa

clean:
	$(RM) -r $(DIR_OBJ)/*.o

.PHONY: all clean clean-full