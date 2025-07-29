CC := gcc
AR := ar

ROOT := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))

ifeq ($(OS),Windows_NT)
    LIB := glenv.lib
    LIBGL := -lopengl32 -lglu32 -lgdi32 -luser32 -lkernel32 -lm
else
    LIB := libglenv.a
    LIBGL := -lGL -lX11 -lXrandr -lm
endif

CFLAGS := -isystem$(CURDIR)
CFLAGSGL := -DGLEW_NO_GLU -DGLEW_STATIC

all: $(LIB)

get_obj_flags:
	$(info -isystem$(ROOT) -fgnu89-inline $(CFLAGSGL))

get_bin_flags:
	$(info -isystem$(ROOT) -L$(ROOT) -lglenv $(LIBGL) -fgnu89-inline $(CFLAGSGL))

$(LIB): glenv.o glew.o
	$(AR) rcs $@ $^

glenv.o: glenv.c
	$(CC) $(CFLAGS) $(CFLAGSGL) -c $< -o $@

glew.o: glew.c
	$(CC) $(CFLAGS) $(CFLAGSGL) -c $< -o $@ $(LIBGL)

clean:
	$(RM) *.o

.PHONY: all clean get_obj_flags get_bin_flags