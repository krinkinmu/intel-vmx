#include <scheduler.h>
#include <string.h>
#include <percpu.h>
#include <paging.h>
#include <thread.h>
#include <hazptr.h>
#include <alloc.h>
#include <debug.h>
#include <apic.h>
#include <time.h>
#include <fpu.h>
#include <cpu.h>
#include <rcu.h>


struct tss {
	uint8_t reserved[102];
	uint16_t iomap_offs;
	uint8_t iomap[1];
} __attribute__((packed));

struct gdt {
	uint64_t null;
	uint64_t code;
	uint64_t data;
	uint64_t tss_low;
	uint64_t tss_high;
} __attribute__((packed));


static void tss_segment_setup(struct tss *tss)
{
	memset(tss, 0, sizeof(*tss));
	tss->iomap_offs = offsetof(struct tss, iomap);
	tss->iomap[0] = 0xff;
}

static void gdt_setup(struct gdt *gdt, struct tss *tss)
{
	const uint64_t tss_base = (uint64_t)tss;
	const uint64_t tss_limit = sizeof(*tss) - 1;

	gdt->null = 0x0ull;
	gdt->code = 0x00209b0000000000ull;
	gdt->data = 0x0020930000000000ull;
	gdt->tss_low = (tss_limit & 0x00ffffull)
			| ((tss_limit & 0xff0000ull) << 32)
			| ((tss_base & 0xffffffull) << 16)
			| (137ull << 40);
	gdt->tss_high = (tss_base >> 32) & 0xffffffffull;

	tss_segment_setup(tss);
}

static void gdt_cpu_setup(void)
{
	const size_t gdt_size = sizeof(struct gdt);
	const size_t tss_size = sizeof(struct tss);

	struct gdt *gdt = mem_alloc(gdt_size + tss_size);
	BUG_ON(!gdt);
	struct tss *tss = (struct tss *)(gdt + 1);

	const struct desc_ptr ptr = {
		.base = (uint64_t)gdt, .limit = gdt_size - 1
	};

	gdt_setup(gdt, tss);
	write_gdt(&ptr);
	write_tr(KERNEL_TSS);
}

static __percpu int this_cpu_id;

static void id_cpu_setup(void)
{
	const int apic_id = local_apic_id();

	for (int i = 0; i != local_apics; ++i) {
		if (apic_id == local_apic_ids[i]) {
			this_cpu_id = i;
			return;
		}
	}
	BUG("Failed to find local apic id %d\n", apic_id);
}

int cpu_id(void)
{
	return this_cpu_id;
}

int cpu_count(void)
{
	return local_apics;
}

void cpu_setup(void)
{
	paging_cpu_setup();
	percpu_cpu_setup();
	id_cpu_setup();
	hp_cpu_setup();
	rcu_cpu_setup();
	fpu_cpu_setup();
	gdt_cpu_setup();
	threads_cpu_setup();
	scheduler_cpu_setup();
	ints_cpu_setup();
	time_cpu_setup();
	local_int_enable();
}
