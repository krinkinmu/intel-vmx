#include <balloc.h>
#include <debug.h>
#include <string.h>
#include <ioport.h>
#include <apic.h>
#include <time.h>
#include <ints.h>
#include <cpu.h>
#include <vmx.h>

#define CMOS_PORT(x) (0x70 + x)

struct tr_data {
	uint32_t magic;
	uint32_t pgtable;
	uint32_t stackend;
	uint32_t startup;
	struct desc_ptr gdt;
	struct desc_ptr idt;
} __attribute__((packed));

static volatile int ap_started;	/* TODO: must be atomic */
static volatile int ap_continue;

static void ap_boot(void)
{
	ap_started = 1;
	while (!ap_continue)
		cpu_relax();

	ints_cpu_setup();
	time_cpu_setup();
	vmx_setup();
	local_int_enable();
	printf("ap started\n");
	while (1)
		cpu_relax();
}

static void tr_data_setup(struct tr_data *data)
{
	uint64_t pgtable;

	pgtable = read_cr3();
	BUG_ON(pgtable & (0xffffffffull << 32));
	data->pgtable = pgtable;
	data->startup = (uintptr_t)&ap_boot;
}

static void cmos_write(unsigned char reg, unsigned char val)
{
	out8(CMOS_PORT(0), reg);
	out8(CMOS_PORT(1), val);
}

static void tr_setup(unsigned long trampoline)
{
	volatile uint16_t *startup_vector = (volatile uint16_t *)0x467;

	*startup_vector = trampoline & 0xf;
	*(startup_vector + 1) = trampoline >> 4;
	cmos_write(0x0f, 0x0a);
}

static void startup_ap(int apic_id, unsigned long startup)
{
	ap_started = 0;

	local_apic_icr_write(apic_id, APIC_ICR_LEVEL | APIC_ICR_ASSERT
				| APIC_ICR_INIT);
	udelay(10000);
	local_apic_icr_write(apic_id, APIC_ICR_LEVEL | APIC_ICR_INIT);
	local_apic_icr_write(apic_id, APIC_ICR_STARTUP | startup >> 12);
	udelay(300);
	local_apic_icr_write(apic_id, APIC_ICR_STARTUP | startup >> 12);

	/* TODO: limit wait and drop AP if startup failed */
	while (!ap_started)
		cpu_relax();
}

void smp_early_setup(void)
{
	static const uintptr_t from = 0x1000;
	static const uintptr_t to = 0xa0000;
	extern char tr_begin[];
	extern char tr_end[];

	if (local_apics == 1)
		return;

	const size_t size = (uintptr_t)tr_end - (uintptr_t)tr_begin;
	struct tr_data *tr_data = 0; 
	char *trampoline = (char *)__balloc_alloc(size, /* align = */0x1000,
				from, to);

	BUG_ON((uintptr_t)trampoline == to);
	printf("place trampoline at 0x%lx\n", (unsigned long)trampoline);
	memcpy(trampoline, tr_begin, size);

	for (char *ptr = trampoline; ptr < trampoline + size; ptr += 4) {
		const uint32_t magic = *((uint32_t *)ptr);

		if (magic == 0x13131313ul) {
			tr_data = (struct tr_data *)ptr;
			break;
		}
	}
	BUG_ON(!tr_data);
	tr_data_setup(tr_data);
	tr_setup((unsigned long)trampoline);

	for (int i = 0; i != local_apics; ++i) {
		static const size_t stack_size = 0x1000;
		static const uintptr_t stack_from = 0x1000;
		static const uintptr_t stack_to = ((uintptr_t)1 << 32) - 1;

		const int apic_id = local_apic_ids[i];

		if (apic_id == local_apic_id())
			continue;

		const uintptr_t stack = __balloc_alloc(stack_size,
					/* align = */0x1000,
					stack_from, stack_to);

		BUG_ON(stack == stack_to); 
		printf("stack for APIC id %d at 0x%lx\n", apic_id,
					(unsigned long)stack);
		tr_data->stackend = stack + stack_size;
		gdt_cpu_create(&tr_data->gdt);
		startup_ap(apic_id, (unsigned long)trampoline);
	}
	balloc_free((uintptr_t)trampoline, (uintptr_t)trampoline + size);
}

void smp_setup(void)
{
	ap_continue = 1;
}
