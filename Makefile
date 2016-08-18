CC ?= gcc
LD ?= ld
AR ?= ar

CFLAGS := -g -m64 -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -ffreestanding \
	-mcmodel=small -Wall -Wextra -Werror -pedantic -std=c99 \
	-Wframe-larger-than=1024 -Wstack-usage=1024 -Wno-unknown-warning-option
LFLAGS := -nostdlib -z max-page-size=0x1000

SRC := src/main.c src/uart8250.c src/print.c
OBJ := $(SRC:.c=.o)
DEP := $(SRC:.c=.d)

ASM := src/bootstrap.S
AOBJ:= $(ASM:.S=.o)
ADEP:= $(ASM:.S=.d)

# ACPICA related defenitions, need to move them in a separate file
ACPICA_SRC		:= acpica/source
ACPICA_OSL		:= $(ACPICA_SRC)/os_specific/service_layers
ACPICA_CORE		:= $(ACPICA_SRC)/components
ACPICA_INC		:= $(ACPICA_SRC)/include
ACPICA_DISPATCHER	:= $(ACPICA_CORE)/dispatcher
ACPICA_EVENTS		:= $(ACPICA_CORE)/events
ACPICA_EXECUTER		:= $(ACPICA_CORE)/executer
ACPICA_HARDWARE		:= $(ACPICA_CORE)/hardware
ACPICA_NAMESPACE	:= $(ACPICA_CORE)/namespace
ACPICA_PARSER		:= $(ACPICA_CORE)/parser
ACPICA_RESOURCES	:= $(ACPICA_CORE)/resources
ACPICA_TABLES		:= $(ACPICA_CORE)/tables
ACPICA_UTILITIES	:= $(ACPICA_CORE)/utilities

ACPICA_SOURCES		:= \
	$(wildcard $(ACPICA_DISPATCHER)/*.c) \
	$(wildcard $(ACPICA_EVENTS)/*.c) \
	$(wildcard $(ACPICA_EXECUTER)/*.c) \
	$(wildcard $(ACPICA_HARDWARE)/*.c) \
	$(wildcard $(ACPICA_NAMESPACE)/*.c) \
	$(wildcard $(ACPICA_PARSER)/*.c) \
	$(filter-out $(ACPICA_RESOURCES)/rsdump.c, $(wildcard $(ACPICA_RESOURCES)/*.c)) \
	$(wildcard $(ACPICA_TABLES)/*.c) \
	$(wildcard $(ACPICA_UTILITIES)/*.c) \
	$(ACPICA_OSL)/osvmx.c
ACPICA_OBJ		:= $(ACPICA_SOURCES:.c=.o)
ACPICA_DEP		:= $(ACPICA_SOURCES:.c=.d)

# libc related defenitions, need to move them in a separate file
LIBC_SRC		:= libc/src
LIBC_INC		:= libc/inc
LIBC_SOURCES		:= $(wildcard $(LIBC_SRC)/*.c)
LIBC_OBJ		:= $(LIBC_SOURCES:.c=.o)
LIBC_DEP		:= $(LIBC_SOURCES:.c=.d)

all: kernel

kernel: $(AOBJ) $(OBJ) libacpica.a libc.a kernel.ld
	$(LD) $(LFLAGS) -T kernel.ld -o $@ $(AOBJ) $(OBJ) -L. -lacpica -lc

libacpica.a: $(ACPICA_OBJ)
	$(AR) rcs $@ $(ACPICA_OBJ)

libc.a: $(LIBC_OBJ)
	$(AR) rcs $@ $(LIBC_OBJ)

$(ACPICA_OBJ): %.o: %.c
	$(CC) -D__VMX__ -DACPI_LIBRARY -Iinc -I$(LIBC_INC) \
		-isystem $(ACPICA_INC) \
		$(CFLAGS) -Wno-format -Wno-unused-parameter \
		-Wno-format-pedantic -MD -c $< -o $@

$(LIBC_OBJ): %.o: %.c
	$(CC) -I$(LIBC_INC) $(CFLAGS) -MD -c $< -o $@

$(AOBJ): %.o: %.S
	$(CC) -D__ASM_FILE__ -g -MD -c $< -o $@

$(OBJ): %.o: %.c
	$(CC) -Iinc -I$(LIBC_INC) -isystem $(ACPICA_INC) \
		$(CFLAGS) -MD -c $< -o $@

-include $(DEP)
-include $(ADEP)
-include $(ACPICA_DEP)
-include $(LIBC_DEP)

.PHONY: clean
clean:
	rm -f kernel $(AOBJ) $(OBJ) $(DEP) $(ADEP) $(ACPICA_OBJ) \
		$(ACPICA_DEP) $(LIBC_OBJ) $(LIBC_DEP) libacpica.a libc.a
