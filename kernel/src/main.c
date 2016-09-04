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
	printf("Guest started\n");
	while (1);
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
