#include <ioport.h>
#include <debug.h>
#include <apic.h>
#include <acpi.h>
#include <cpu.h>


#define MAX_IO_APICS	8
#define MAX_LOCAL_APICS	MAX_CPU_NR

struct io_apic ioapic[MAX_IO_APICS];
int ioapics;

static unsigned long local_apic_phys;
int local_apic_ids[MAX_LOCAL_APICS];
int local_apics;


struct acpi_madt {
	ACPI_TABLE_HEADER hdr;
	uint32_t local_apic_phys;
	uint32_t flags;
} __attribute__((packed));

struct acpi_apic_hdr {
	uint8_t type;
	uint8_t size;
} __attribute__((packed));

struct acpi_local_apic {
	struct acpi_apic_hdr hdr;
	uint8_t acpi_cpu_id;
	uint8_t local_apic_id;
	uint32_t flags;
} __attribute__((packed));

struct acpi_io_apic {
	struct acpi_apic_hdr hdr;
	uint8_t io_apic_id;
	uint8_t reserved;
	uint32_t io_apic_phys;
	uint32_t intno;
} __attribute__((packed));

struct acpi_local_apic_phys {
	struct acpi_apic_hdr hdr;
	uint16_t reserved;
	uint64_t local_apic_phys;
} __attribute__((packed));


static void apic_enumerate_acpi(void)
{
	ACPI_TABLE_HEADER *table;
	ACPI_STATUS status = AcpiGetTable("APIC", 1, &table);

	if (ACPI_FAILURE(status))
		BUG("Failed to get MADT table\n");

	const struct acpi_madt *madt = (const struct acpi_madt *)table;
	const size_t size = madt->hdr.Length;
	uintptr_t ptr = (uintptr_t)(madt + 1);
	const uintptr_t end = ptr + size;

	local_apic_phys = (unsigned long)madt->local_apic_phys;
	while (ptr < end) {
		const struct acpi_apic_hdr *hdr =
					(const struct acpi_apic_hdr *)ptr;

		switch (hdr->type) {
		case 0: {
			const struct acpi_local_apic *apic =
					(const struct acpi_local_apic *)hdr;

			if ((apic->flags & 1) == 0)
				break;

			if (local_apics == MAX_LOCAL_APICS)
				break;
			local_apic_ids[local_apics++] = apic->local_apic_id;
			break;
		}
		case 1: {
			const struct acpi_io_apic *io =
					(const struct acpi_io_apic *)hdr;
			struct io_apic *apic = &ioapic[ioapics];

			BUG_ON(ioapics == MAX_IO_APICS);
			apic->base_gsi = io->intno;
			apic->phys = io->io_apic_phys;
			++ioapics;
			break;
		}
		case 5: {
			const struct acpi_local_apic_phys *phys =
				(const struct acpi_local_apic_phys *)hdr;

			local_apic_phys = (unsigned long)phys->local_apic_phys;
			break;
		}
		}
		ptr += hdr->size;
	}
}

static void disable_legacy_pic(void)
{
	/* We only need to disable legacy pic, so leave it
	 * as a piece of magic while it's work well. */
	static const int master_cmd_port = 0x20;
	static const int master_data_port = 0x21;
	static const int slave_cmd_port = 0xa0;
	static const int slave_data_port = 0xa1;

	out8(master_cmd_port, (1 << 0) | (1 << 4));
	out8(slave_cmd_port, (1 << 0) | (1 << 4));
	out8(master_data_port, 0x20);
	out8(slave_data_port, 0x28);
	out8(master_data_port, (1 << 2));
	out8(slave_data_port, 2);
	out8(master_data_port, 1);
	out8(slave_data_port, 1);
	out8(master_data_port, 0xff);
	out8(slave_data_port, 0xff);
}

void io_apic_write(struct io_apic *apic, int reg, unsigned long val)
{
	volatile uint32_t *base = (volatile uint32_t *)apic->phys;

	*base = reg;
	*(base + 4) = val;
}

unsigned long io_apic_read(const struct io_apic *apic, int reg)
{
	volatile uint32_t *base = (volatile uint32_t *)apic->phys;

	*base = reg;
	return *(base + 4);
}

static void ioapic_setup(struct io_apic *apic)
{
	const unsigned long ioapicver = io_apic_read(apic, 1);
	const int last_io_pin = (ioapicver >> 16) & 0xff;

	apic->pins = last_io_pin + 1;

	for (int i = 0; i != apic->pins; ++i)
		/* By default they should be disabled, but anyway... */
		io_apic_write(apic, IO_APIC_RDREG_LOW(i), IO_APIC_MASK_PIN);
}

void local_apic_write(int reg, unsigned long val)
{
	volatile uint32_t *ptr = (volatile uint32_t *)(local_apic_phys + reg);

	*ptr = val;
}

unsigned long local_apic_read(int reg)
{
	volatile uint32_t *ptr = (volatile uint32_t *)(local_apic_phys + reg);

	return *ptr;
}

int local_apic_id(void)
{
	const unsigned long apic_id = local_apic_read(0x20);

	return (apic_id >> 24) & 0xff;
}

int local_apic_version(void)
{
	const unsigned long apic_version = local_apic_read(0x30);

	return apic_version & 0xff;
}

void local_apic_icr_write(int dest, unsigned long flags)
{
	local_apic_write(0x310, (unsigned long)dest << 24);
	local_apic_write(0x300, flags);

	while (local_apic_read(0x300) & APIC_ICR_PENDING)
		cpu_relax();
}

static void lapic_setup(void)
{
	static int next_apic_id;
	const unsigned long apic_id = 1ul << (next_apic_id++ % local_apics);

	local_apic_write(0xe0, 15ul << 28);
	local_apic_write(0xd0, apic_id << 24);
}

struct io_apic *io_apic_find(int gsi)
{
	for (int i = 0; i != ioapics; ++i) {
		struct io_apic *apic = &ioapic[i];

		if (apic->base_gsi <= gsi && apic->base_gsi + apic->pins > gsi)
			return apic;
	}

	return 0;
}

int io_apic_pin(const struct io_apic *apic, int gsi)
{
	return gsi - apic->base_gsi;
}

void apic_setup(void)
{
	apic_enumerate_acpi();
	disable_legacy_pic();
	for (int i = 0; i != ioapics; ++i)
		ioapic_setup(&ioapic[i]);
	lapic_setup();


	printf("There are %d local APICS at addr 0x%lx\n",
			local_apics, (unsigned long)local_apic_phys);

	for (int i = 0; i != ioapics; ++i) {
		const struct io_apic *apic = &ioapic[i];

		printf("IO APIC id ints [%d;%d] at addr 0x%lx\n",
					apic->base_gsi,
					apic->base_gsi + apic->pins - 1,
					(unsigned long)apic->phys);
	}
}
