#include <time.h>
#include <acpi.h>
#include <debug.h>
#include <stdint.h>


struct acpi_addr {
	uint8_t address_space;
	uint8_t width;
	uint8_t offset;
	uint8_t size;
	uint64_t addr;
} __attribute__((packed));

struct acpi_hpet {
	uint32_t sign;
	uint32_t size;
	uint8_t revision;
	uint8_t chksum;
	uint8_t oem_id[6];
	uint64_t oem_table_id;
	uint32_t oem_revision;
	uint32_t creator_id;
	uint32_t creator_revision;
	uint32_t hpet_block_id;
	struct acpi_addr addr;
	uint8_t hpet_seq_num;
	uint16_t min_clock_ticks;
	uint8_t attrs;
} __attribute__((packed));

static unsigned long long hpet_period_us;
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

	hpet_addr = (unsigned long)hpet->addr.addr;
}

static void hpet_setup(void)
{
	BUG_ON((hpet_read(0x00) & (1 << 13)) == 0);
	hpet_period_us = 1;//hpet_read(0x04);// * 1000000000ull;
	hpet_write(0x10, 1);
}

void udelay(unsigned long usec)
{
	const unsigned long long start = hpet_read64(0xf0);
	const unsigned long long until = start + usec * hpet_period_us;

	while (1) {
		const unsigned long long clk = hpet_read64(0xf0);

		if (until < start) {
			if (clk <= start && clk >= until)
				break;
		} else {
			if (clk >= until || clk <= start)
				break;
		}

		__asm__ volatile ("pause");
	}
}

void time_setup(void)
{
	hpet_enumerate();
	hpet_setup();
}
