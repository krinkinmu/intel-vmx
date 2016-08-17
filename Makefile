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
ACPICA_INCLUDE		:= $(ACPICA_SRC)/include
ACPICA_DISPATCHER	:= $(ACPICA_CORE)/dispatcher
ACPICA_EVENTS		:= $(ACPICA_CORE)/events
ACPICA_EXECUTER		:= $(ACPICA_CORE)/executer
ACPICA_HARDWARE		:= $(ACPICA_CORE)/hardware
ACPICA_NAMESPACE	:= $(ACPICA_CORE)/namespace
ACPICA_PARSER		:= $(ACPICA_CORE)/parser
ACPICA_RESOURCES	:= $(ACPICA_CORE)/resources
ACPICA_TABLES		:= $(ACPICA_CORE)/tables
ACPICA_UTILITIES	:= $(ACPICA_CORE)/utilities

ACPICA_HEADERS		:= \
	$(wildcard $(ACPICA_INCLUDE)/*.h) \
	$(wildcard $(ACPICA_INCLUDE)/platfrom/*.h)

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
ACPICA_OBJECTS		:= $(ACPICA_SOURCES:.c=.o)

all: kernel

kernel: $(AOBJ) $(OBJ) libacpica.a kernel.ld
	$(LD) $(LFLAGS) -T kernel.ld -o $@ $(AOBJ) $(OBJ) -L. -lacpica

libacpica.a: $(ACPICA_OBJECTS)
	$(AR) rcs $@ $(ACPICA_OBJECTS)

$(ACPICA_OBJECTS): %.o: %.c $(ACPICA_HEADERS)
	$(CC) -D__VMX__ -DACPI_LIBRARY -Iinc -isystem $(ACPICA_INCLUDE) \
		$(CFLAGS) -Wno-format -Wno-unused-parameter \
		-Wno-format-pedantic -c $< -o $@

$(AOBJ): %.o: %.S
	$(CC) -D__ASM_FILE__ -g -MMD -c $< -o $@

$(OBJ): %.o: %.c
	$(CC) -Iinc -isystem $(ACPICA_INCLUDE) $(CFLAGS) -MMD -c $< -o $@

-include $(DEP)
-include $(ADEP)

.PHONY: clean
clean:
	rm -f kernel $(AOBJ) $(OBJ) $(DEP) $(ADEP) $(ACPICA_OBJECTS) libacpica.a
