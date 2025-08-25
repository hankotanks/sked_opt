CC := gcc
AR := ar

# TODO: is_nan check, see ./lpsolve55/ccc
# TODO: platform specific flags, etc?

ROOT := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))

ifeq ($(OS),Windows_NT)
    LIB := lpsolve.lib
#	IS_NAN_CHECK := $(ROOT)/is_nan.exe
else
    LIB := liblpsolve.a
#    IS_NAN_CHECK := $(ROOT)/is_nan
endif

SRC := \
	$(ROOT)/lp_MDO.c \
	$(ROOT)/shared/commonlib.c \
	$(ROOT)/colamd/colamd.c \
	$(ROOT)/shared/mmio.c \
	$(ROOT)/shared/myblas.c \
	$(ROOT)/ini.c \
	$(ROOT)/fortify.c \
	$(ROOT)/lp_rlp.c \
	$(ROOT)/lp_crash.c \
	$(ROOT)/bfp/bfp_LUSOL/lp_LUSOL.c \
	$(ROOT)/bfp/bfp_LUSOL/LUSOL/lusol.c \
	$(ROOT)/lp_Hash.c \
	$(ROOT)/lp_lib.c \
	$(ROOT)/lp_wlp.c \
	$(ROOT)/lp_matrix.c \
	$(ROOT)/lp_mipbb.c \
	$(ROOT)/lp_MPS.c \
	$(ROOT)/lp_params.c \
	$(ROOT)/lp_presolve.c \
	$(ROOT)/lp_price.c \
	$(ROOT)/lp_pricePSE.c \
	$(ROOT)/lp_report.c \
	$(ROOT)/lp_scale.c \
	$(ROOT)/lp_simplex.c \
	$(ROOT)/lp_SOS.c \
	$(ROOT)/lp_utils.c \
	$(ROOT)/yacc_read.c 

OBJ := $(SRC:.c=.o)

CFLAGS := \
	-I$(ROOT) \
	-I$(ROOT)/shared \
	-I$(ROOT)/bfp \
	-I$(ROOT)/bfp/bfp_etaPFI \
	-I$(ROOT)/bfp/bfp_LUSOL \
	-I$(ROOT)/bfp/bfp_LUSOL/LUSOL \
	-I$(ROOT)/colamd

LDLIBS := -lm

all: $(OBJ)
	$(AR) rcs $(LIB) $^

get_obj_flags:
	$(info -isystem$(ROOT))

get_bin_flags:
	$(info -isystem$(ROOT) -L$(ROOT) -llpsolve -lm)

%.o: %.c IS_NAN_CHECK
# 	@$(IS_NAN_CHECK)
# ifeq ($(OS),Windows_NT)
# 	@if %ERRORLEVEL% neq 0 ($(eval CFLAGS += -DNOISNAN))
# else
# 	@if [ $$? -ne 0 ]; then $(eval CFLAGS += -DNOISNAN); fi
# endif
	$(CC) $(CFLAGS) -c $< -o $@ $(LDLIBS)

# IS_NAN_CHECK: is_nan.c
# 	$(CC) $< -o $@ -lm

clean:
	$(RM) $(OBJ)

.PHONY: all clean get_obj_flags get_bin_flags