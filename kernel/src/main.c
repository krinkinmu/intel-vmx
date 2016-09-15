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

static void gdb_hang(void)
{
#ifdef DEBUG
	static volatile int wait = 1;
	while (wait);
#endif
}

static void gdt_boot_cpu_setup(void)
{
	struct desc_ptr ptr;

	gdt_cpu_create(&ptr);
	gdt_cpu_setup(&ptr);
}

void main(const struct mboot_info *info)
{
	gdb_hang();
	uart8250_setup();
	balloc_setup(info);
	gdt_boot_cpu_setup();
	acpi_early_setup();
	apic_setup();
	ints_setup();
	ints_cpu_setup();
	local_int_enable();
	time_setup();
	time_cpu_setup();
	smp_early_setup();
	page_alloc_setup();
	smp_setup();
	vmx_setup();

	while (1);
}
