#include <string.h>
#include <cpu.h>

extern char tss[];

static void tss_segment_setup(void)
{
	volatile struct tss *ptr = (struct tss *)tss;

	memset((void *)tss, 0, sizeof(*ptr));
	ptr->iomap_offs = offsetof(struct tss, iomap);
	ptr->iomap[0] = 0xff;
}

static void tss_descriptor_setup(void)
{
	struct desc_ptr ptr;
	const int index = (KERNEL_TSS >> 3);
	const uint64_t limit = sizeof(struct tss) - 1;
	const uint64_t base = (uint64_t)tss;
	volatile uint64_t *desc;

	read_gdt(&ptr);
	desc = (volatile uint64_t *)ptr.base;
	desc[index] = (limit & 0x00ffffull) | ((limit & 0xff0000ull) << 32)
		| ((base & 0xffffffull) << 16) | (137ull << 40);
	desc[index + 1] = (base >> 32) & 0xffffffffull;
}

void tss_cpu_setup(void)
{
	write_tr(KERNEL_TSS);
}

void tss_setup(void)
{
	tss_segment_setup();
	tss_descriptor_setup();
}
