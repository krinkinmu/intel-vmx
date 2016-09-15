#include <string.h>
#include <balloc.h>
#include <debug.h>
#include <cpu.h>


struct tss {
	uint8_t reserved[102];
	uint16_t iomap_offs;
	uint8_t iomap[1];
} __attribute__((packed));


static void tss_segment_setup(struct tss *tss)
{
	memset(tss, 0, sizeof(*tss));
	tss->iomap_offs = offsetof(struct tss, iomap);
	tss->iomap[0] = 0xff;
}

static void tss_setup(uint64_t *gdt, struct tss *tss)
{
	const uint64_t base = (uint64_t)tss;
	const uint64_t limit = sizeof(*tss) - 1;

	tss_segment_setup(tss);

	gdt[3] = (limit & 0x00ffffull) | ((limit & 0xff0000ull) << 32)
		| ((base & 0xffffffull) << 16) | (137ull << 40);
	gdt[4] = (base >> 32) & 0xffffffffull;
}

static void gdt_setup(uint64_t *gdt, struct tss *tss)
{
	gdt[0] = 0x0ull;
	gdt[1] = 0x00209b0000000000ull;
	gdt[2] = 0x0020930000000000ull;

	tss_setup(gdt, tss);
}

void gdt_cpu_create(struct desc_ptr *ptr)
{
	static const unsigned long long begin = 0;
	static const unsigned long long end = 1ull << 32;

	const size_t gdt_size = 5 * 8; /* null, code + data, 2 * 8 for tss */
	const size_t tss_size = sizeof(struct tss);
	const uintptr_t gdt_addr = balloc_alloc(gdt_size + tss_size,
				begin, end);

	BUG_ON(gdt_addr == end);

	uint64_t *gdt = (uint64_t *)gdt_addr;
	struct tss *tss = (struct tss *)(gdt_addr + gdt_size);

	gdt_setup(gdt, tss);

	ptr->base = gdt_addr;
	ptr->limit = gdt_size - 1;
}

void gdt_cpu_setup(const struct desc_ptr *ptr)
{
	write_gdt(ptr);
	write_tr(KERNEL_TSS);
}
