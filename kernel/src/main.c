#include <acpi.h>
#include <uart8250.h>
#include <balloc.h>
#include <debug.h>
#include <time.h>
#include <apic.h>
#include <ints.h>
#include <cpu.h>
#include <smpboot.h>
#include <memory.h>
#include <alloc.h>
#include <vmx.h>

static void acpi_early_setup(void)
{
	static ACPI_TABLE_DESC tables[16];
	static const size_t size = sizeof(tables)/sizeof(tables[0]);

	ACPI_STATUS status = AcpiInitializeTables(tables, size, FALSE);
	if (ACPI_FAILURE(status))
		BUG("Failed to initialize ACPICA tables subsytem\n");
}

static void guest_entry(void)
{
	printf("Guest started.\n");
	while (1)
		printf("Guest working...\n");
}

static void mem_cache_test(void)
{
	struct mem_cache cache;
	struct list_head lst;

	mem_cache_setup(&cache, sizeof(lst), sizeof(lst));
	list_init(&lst);
	for (size_t i = 0; i != 1000000; ++i) {
		struct list_head *ptr = mem_cache_alloc(&cache);

		if (!ptr) {
			printf("allocated %lu entries\n", (unsigned long)i);
			break;
		}
		list_add_tail(ptr, &lst);
	}

	for (struct list_head *ptr = lst.next; ptr != &lst;) {
		void *to_free = ptr;

		ptr = ptr->next;
		mem_cache_free(&cache, to_free);
	}
	mem_cache_release(&cache);
}

void main(const struct mboot_info *info)
{
#ifdef DEBUG
	static volatile int wait = 1;
	while (wait);
#endif

	uart8250_setup();
	balloc_setup(info);
	acpi_early_setup();
	apic_setup();
	ints_setup();
	ints_cpu_setup();
	local_int_enable();
	tss_setup();
	tss_cpu_setup();
	time_setup();
	time_cpu_setup();
	smp_early_setup();
	page_alloc_setup();

	mem_cache_test();

	smp_setup();
	vmx_setup();
	vmx_enter();
	
	struct vmx_guest guest;
	static unsigned char stack[4096];

	vmx_guest_setup(&guest);
	guest.entry = (uintptr_t)&guest_entry;
	guest.stack = (uintptr_t)(stack + 4096);
	vmx_guest_run(&guest);
	while (1);
}
