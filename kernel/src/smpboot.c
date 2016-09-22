#include <alloc.h>
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
} __attribute__((packed));

static volatile int ap_started;
static volatile int ap_continue;

static void ap_boot(void)
{
	printf("ap started\n");
	ap_started = 1;
	while (!ap_continue)
		cpu_relax();

	gdt_cpu_setup();
	ints_cpu_setup();
	time_cpu_setup();
	local_int_enable();
	vmx_setup();
	while (1)
		cpu_relax();
}

static void tr_data_setup(struct tr_data *data)
{
	uint64_t pgtable;

	pgtable = read_cr3();
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

static void startup_ap(int apic_id, uint32_t startup)
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

static int smp_trampoline_order(size_t size)
{
	int order = 0;

	while (size > ((size_t)1ul << (PAGE_SHIFT + order)))
		++order;

	return order;
}

static struct tr_data *smp_tr_data_find(char *begin, char *end)
{
	for (char *ptr = begin; ptr < end; ptr += 4) {
		const uint32_t magic = *((uint32_t *)ptr);

		if (magic == 0x13131313ul)
			return (struct tr_data *)ptr;
	}
	return 0;
}

void smp_early_setup(void)
{
	extern char tr_begin[];
	extern char tr_end[];

	if (local_apics == 1)
		return;

	const size_t size = (uintptr_t)tr_end - (uintptr_t)tr_begin;
	const int order = smp_trampoline_order(size);
	struct tr_data *tr_data = 0; 
	char *trampoline = (char *)page_alloc(order, PA_LOW);

	BUG_ON(!trampoline);
	printf("place trampoline at 0x%lx\n", (unsigned long)trampoline);
	memcpy(trampoline, tr_begin, size);

	tr_data = smp_tr_data_find(trampoline, trampoline + size);

	BUG_ON(!tr_data);
	tr_data_setup(tr_data);
	tr_setup((unsigned long)trampoline);

	for (int i = 0; i != local_apics; ++i) {
		const int apic_id = local_apic_ids[i];
		const int order = 0;
		const size_t size = (size_t)1 << (order + PAGE_SHIFT);

		if (apic_id == local_apic_id())
			continue;

		const uintptr_t stack = page_alloc(order, PA_NORMAL);

		BUG_ON(!stack);
		printf("stack for APIC id %d at 0x%lx\n", apic_id,
					(unsigned long)stack);
		tr_data->stackend = stack + size;
		startup_ap(apic_id, (unsigned long)trampoline);
	}
	page_free((uintptr_t)trampoline, order);
}

void smp_setup(void)
{
	ap_continue = 1;
}
