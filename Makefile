CC ?= gcc
LD ?= ld

CFLAGS := -g -m64 -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -ffreestanding \
	-mcmodel=small -Wall -Wextra -Werror -pedantic -std=c99 \
	-Wframe-larger-than=1024 -Wstack-usage=1024 -Wno-unknown-warning-option
LFLAGS := -nostdlib -z max-page-size=0x1000

SRC := main.c
OBJ := $(SRC:.c=.o)
DEP := $(SRC:.c=.d)

ASM := bootstrap.S
AOBJ:= $(ASM:.S=.o)
ADEP:= $(ASM:.S=.d)

all: kernel

kernel: $(AOBJ) $(OBJ) kernel.ld
	$(LD) $(LFLAGS) -T kernel.ld -o $@ $(AOBJ) $(OBJ)

%.o: %.S
	$(CC) -D__ASM_FILE__ -g -MMD -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -MMD -c $< -o $@

-include $(DEP)
-include $(ADEP)

.PHONY: clean
clean:
	rm -f kernel $(AOBJ) $(OBJ) $(DEP) $(ADEP)
