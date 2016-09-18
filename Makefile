CC ?= gcc
LD ?= ld
AR ?= ar

CFLAGS := -g -m64 -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -ffreestanding \
	-mcmodel=small -Wall -Wextra -Werror -pedantic -std=c11 \
	-Wframe-larger-than=1024 -Wstack-usage=1024 \
	-Wno-unknown-warning-option $(if $(DEBUG),-DDEBUG,)
LFLAGS := -nostdlib -z max-page-size=0x1000

KERNEL_SRC		:= kernel/src
KERNEL_INC		:= kernel/inc
KERNEL_C		:= $(wildcard $(KERNEL_SRC)/*.c)
KERNEL_C_OBJ		:= $(KERNEL_C:.c=.o)
KERNEL_C_DEP		:= $(KERNEL_C:.c=.d)
KERNEL_A		:= $(wildcard $(KERNEL_SRC)/*.S)
KERNEL_A_OBJ		:= $(KERNEL_A:.S=.o)
KERNEL_A_DEP		:= $(KERNEL_A:.S=.d)
KERNEL_OBJ		:= $(KERNEL_C_OBJ) $(KERNEL_A_OBJ)
KERNEL_DEP		+= $(KERNEL_C_DEP) $(KERNEL_A_DEP)

# ACPICA related defenitions, need to move them in a separate file
ACPICA_SRC		:= acpica/source
ACPICA_INC		:= $(ACPICA_SRC)/include
ACPICA_OSL		:= $(ACPICA_SRC)/os_specific/service_layers
ACPICA_CORE		:= $(ACPICA_SRC)/components
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
LIBC_C			:= $(wildcard $(LIBC_SRC)/*.c)
LIBC_C_OBJ		:= $(LIBC_C:.c=.o)
LIBC_C_DEP		:= $(LIBC_C:.c=.d)
LIBC_A			:= $(wildcard $(LIBC_SRC)/*.S)
LIBC_A_OBJ		:= $(LIBC_A:.S=.o)
LIBC_A_DEP		:= $(LIBC_A:.S=.d)
LIBC_OBJ		:= $(LIBC_C_OBJ) $(LIBC_A_OBJ)
LIBC_DEP		:= $(LIBC_C_DEP) $(LIBC_A_DEP)

all: kernel/kernel

kernel/kernel: $(KERNEL_OBJ) libacpica.a libc.a kernel/kernel.ld
	$(LD) $(LFLAGS) -T kernel/kernel.ld -o $@ $(KERNEL_OBJ) -L. -lacpica -lc

libacpica.a: $(ACPICA_OBJ)
	$(AR) rcs $@ $(ACPICA_OBJ)

libc.a: $(LIBC_OBJ)
	$(AR) rcs $@ $(LIBC_OBJ)

$(ACPICA_OBJ): %.o: %.c
	$(CC) -D__VMX__ -DACPI_LIBRARY -I$(KERNEL_INC) -I$(LIBC_INC) \
		-isystem $(ACPICA_INC) \
		$(CFLAGS) -Wno-format -Wno-unused-parameter \
		-Wno-format-pedantic -MD -c $< -o $@

$(LIBC_A_OBJ): %.o: %.S
	$(CC) -D__ASM_FILE__ -g -MD -c $< -o $@

$(LIBC_C_OBJ): %.o: %.c
	$(CC) -I$(LIBC_INC) $(CFLAGS) -O2 -MD -c $< -o $@

$(KERNEL_A_OBJ): %.o: %.S
	$(CC) -D__ASM_FILE__ -g -MD -c $< -o $@

$(KERNEL_C_OBJ): %.o: %.c
	$(CC) -I$(KERNEL_INC) -I$(LIBC_INC) -isystem $(ACPICA_INC) \
		$(CFLAGS) -O2 -MD -c $< -o $@

-include $(KERNEL_DEP)
-include $(ACPICA_DEP)
-include $(LIBC_DEP)

.PHONY: clean
clean:
	rm -f kernel/kernel $(KERNEL_OBJ) $(KERNEL_DEP) $(ACPICA_OBJ) \
		$(ACPICA_DEP) $(LIBC_OBJ) $(LIBC_DEP) libacpica.a libc.a
