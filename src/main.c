#include <acpi.h>
#include <uart8250.h>
#include <print.h>

static void acpi_early_init(void)
{
	static ACPI_TABLE_DESC tables[16];

	AcpiInitializeTables(tables, sizeof(tables)/sizeof(tables[0]), FALSE);
}

void main(void)
{
	uart8250_setup();
	printf("Hello from %d bit bare metal %s\n", 64, "application");
	acpi_early_init();
	while (1);
}
