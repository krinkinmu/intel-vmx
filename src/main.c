#include <acpi.h>
#include <uart8250.h>


static void acpi_early_init(void)
{
	static ACPI_TABLE_DESC tables[16];

	AcpiInitializeTables(tables, sizeof(tables)/sizeof(tables[0]), FALSE);
}

void main(void)
{
	uart8250_setup();
	uart8250_write("Hello", sizeof("Hello") - 1);
	acpi_early_init();
	while (1);
}
