#include <acpi.h>
#include <uart8250.h>
#include <print.h>
#include <stdint.h>

#define BUG(...) \
	do { \
		printf(__VA_ARGS__); \
		while (1); \
	} while (0)

static void acpi_early_init(void)
{
	static ACPI_TABLE_DESC tables[16];
	static const size_t size = sizeof(tables)/sizeof(tables[0]);

	ACPI_STATUS status = AcpiInitializeTables(tables, size, FALSE);
	if (ACPI_FAILURE(status))
		BUG("Failed to initialize ACPICA tables subsytem\n");
}

struct acpi_madt {
	uint32_t sign;
	uint32_t size;
	uint8_t revision;
	uint8_t chksum;
	uint8_t oem_id[6];
	uint64_t oem_table_id;
	uint32_t oem_revision;
	uint32_t creator_id;
	uint32_t creator_revision;
	uint32_t local_apic_phys;
	uint32_t flags;
} __attribute__((packed));

struct acpi_apic {
	uint8_t type;
	uint8_t size;
} __attribute__((packed));

struct acpi_local_apic {
	struct acpi_apic hdr;
	uint8_t acpi_cpu_id;
	uint8_t local_apic_id;
	uint32_t flags;
} __attribute__((packed));

struct acpi_io_apic {
	struct acpi_apic hdr;
	uint8_t io_apic_id;
	uint8_t reserved;
	uint32_t io_apic_phys;
	uint32_t intno;
} __attribute__((packed));

struct acpi_source {
	struct acpi_apic hdr;
	uint8_t bus;
	uint8_t src;
	uint32_t intno;
	uint16_t flags;
} __attribute__((packed));

struct acpi_nmi_io {
	struct acpi_apic hdr;
	uint16_t flags;
	uint32_t intno;
} __attribute__((packed));

struct acpi_nmi_local {
	struct acpi_apic hdr;
	uint8_t acpi_cpu_id;
	uint16_t flags;
	uint8_t lint;
} __attribute__((packed));

struct acpi_local_apic_phys {
	struct acpi_apic hdr;
	uint16_t reserved;
	uint64_t local_apic_phys;
} __attribute__((packed));

struct acpi_local_x2apic {
	struct acpi_apic hdr;
	uint16_t reserved;
	uint32_t local_apic_id;
	uint32_t flags;
	uint32_t acpi_cpu_uid;
} __attribute__((packed));

struct acpi_nmi_x2local {
	struct acpi_apic hdr;
	uint16_t flags;
	uint32_t acpi_cpu_uid;
	uint8_t lint;
	uint8_t reserved[3];
} __attribute__((packed));


static void acpi_enumerate_apics(void)
{
	ACPI_TABLE_HEADER *table;
	ACPI_STATUS status;

	status = AcpiGetTable("APIC", 1, &table);
	if (ACPI_FAILURE(status))
		BUG("Failed to get MADT table\n");

	const struct acpi_madt *madt = (const struct acpi_madt *)table;
	const size_t size = madt->size;
	uintptr_t ptr = (uintptr_t)(madt + 1);
	uintptr_t end = ptr + size;
	int local_apic_phys_changed = 0;

	while (ptr < end) {
		const struct acpi_apic *apic = (const struct acpi_apic *)ptr;

		switch (apic->type) {
		case 0: {
			const struct acpi_local_apic *local =
					(const struct acpi_local_apic *)apic;

			if ((local->flags & 1) == 0)
				break;
			printf("Local APIC id %d (ACPI id %d)\n",
					(int)local->local_apic_id,
					(int)local->acpi_cpu_id);
			break;
		}
		case 1: {
			const struct acpi_io_apic *io =
					(const struct acpi_io_apic *)apic;

			printf("IO APIC id %d int %lu at %lx\n",
					(int)io->io_apic_id,
					(unsigned long)io->intno,
					(unsigned long)io->io_apic_phys);
			break;
		}
		case 2: {
			const struct acpi_source *src =
					(const struct acpi_source *)apic;

			printf("ISA int %d mapped to intno %lu\n",
					(int)src->src,
					(unsigned long)src->intno);
			break;
		}
		case 3: {
			const struct acpi_nmi_io *nmi =
					(const struct acpi_nmi_io *)apic;

			printf("NMI at intno %lu\n",
					(unsigned long)nmi->intno);
			break;
		}
		case 4: {
			const struct acpi_nmi_local *nmi =
					(const struct acpi_nmi_local *)apic;

			printf("Local NMI LINT %d ", (int)nmi->lint);
			if (nmi->acpi_cpu_id != 0xff)
				printf("(ACPI id %d)\n",
					(unsigned long)nmi->acpi_cpu_id);
			else
				printf("(for all CPUs)\n");
			break;
		}
		case 5: {
			const struct acpi_local_apic_phys *phys =
				(const struct acpi_local_apic_phys *)apic;

			local_apic_phys_changed = 1;
			printf("Local APIC phys %lx\n",
					(unsigned long)phys->local_apic_phys);
			break;
		}
		case 9: {
			const struct acpi_local_x2apic *local =
				(const struct acpi_local_x2apic *)apic;

			if ((local->flags & 1) == 0)
				break;
			printf("Local X2APIC id %d (ACPI id %lu)\n",
					(int)local->local_apic_id,
					(unsigned long)local->acpi_cpu_uid);
			break;
		}
		case 10: {
			const struct acpi_nmi_x2local *nmi =
				(const struct acpi_nmi_x2local *)apic;
			
			printf("Local NMI LINT %d ", (int)nmi->lint);
			if (nmi->acpi_cpu_uid != 0xfffffffful)
				printf("(ACPI id %lu)\n",
					(unsigned long)nmi->acpi_cpu_uid);
			else
				printf("(for all CPUs)\n");
			break;
		}
		}

		ptr += apic->size;
	}


	if (!local_apic_phys_changed)
		printf("Local APIC phys %lx\n",
				(unsigned long)madt->local_apic_phys);
}

void main(void)
{
	uart8250_setup();
	acpi_early_init();
	acpi_enumerate_apics();
	while (1);
}
