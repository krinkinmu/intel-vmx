#include <time.h>
#include <acpi.h>
#include <debug.h>
#include <stdint.h>
#include <cpu.h>


struct acpi_hpet {
	ACPI_TABLE_HEADER hdr;
	uint32_t hpet_block_id;
	ACPI_GENERIC_ADDRESS addr;
	uint8_t hpet_seq_num;
	uint16_t min_clock_ticks;
	uint8_t attrs;
} __attribute__((packed));

static unsigned long long hpet_period;
static unsigned long hpet_addr;

static unsigned long hpet_read(int reg)
{
	volatile uint32_t *ptr = (volatile uint32_t *)(hpet_addr + reg);

	return *ptr;
}

static unsigned long long hpet_read64(int reg)
{
	volatile uint64_t *ptr = (volatile uint64_t *)(hpet_addr + reg);

	return *ptr;
}

static void hpet_write(int reg, unsigned long val)
{
	volatile uint32_t *ptr = (volatile uint32_t *)(hpet_addr + reg);

	*ptr = val;
}

static void hpet_enumerate(void)
{
	ACPI_TABLE_HEADER *table;
	ACPI_STATUS status = AcpiGetTable("HPET", 1, &table);

	if (ACPI_FAILURE(status))
		BUG("Failed to get HPET table\n");

	const struct acpi_hpet *hpet = (const struct acpi_hpet *)table;

	BUG_ON(hpet->addr.SpaceId != 0);
	hpet_addr = (unsigned long)hpet->addr.Address;
}

static void hpet_setup(void)
{
	BUG_ON((hpet_read(0x00) & (1 << 13)) == 0);
	hpet_period = hpet_read(0x04);
	hpet_write(0x10, 1);
}

void udelay(unsigned long usec)
{
	const unsigned long long clks = (usec * 1000000000ull) / hpet_period;
	const unsigned long long start = hpet_read64(0xf0);
	const unsigned long long until = start + clks;

	while (1) {
		const unsigned long long clk = hpet_read64(0xf0);

		if (until < start) {
			if (clk <= start && clk >= until)
				break;
		} else {
			if (clk >= until || clk <= start)
				break;
		}
		cpu_relax();
	}
}

void time_setup(void)
{
	hpet_enumerate();
	hpet_setup();
}
