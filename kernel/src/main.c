#include <acpi.h>
#include <uart8250.h>
#include <balloc.h>
#include <debug.h>
#include <apic.h>

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
	static volatile int wait = 0;
	while (wait);
	uart8250_setup();
	balloc_setup(info);
	acpi_early_setup();
	apic_setup();
	while (1);
}
