#include <acpi.h>
#include <uart8250.h>
#include <balloc.h>
#include <debug.h>
#include <time.h>
#include <apic.h>
#include <ints.h>
#include <smpboot.h>
#include <memory.h>

static void acpi_early_setup(void)
{
	static ACPI_TABLE_DESC tables[16];
	static const size_t size = sizeof(tables)/sizeof(tables[0]);

	ACPI_STATUS status = AcpiInitializeTables(tables, size, FALSE);
	if (ACPI_FAILURE(status))
		BUG("Failed to initialize ACPICA tables subsytem\n");
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
	cpu_ints_setup();
	time_setup();
	smp_early_setup();
	page_alloc_setup();
	smp_setup();
	local_int_enable();
	while (1);
}
