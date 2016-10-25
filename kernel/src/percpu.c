#include <percpu.h>
#include <memory.h>
#include <balloc.h>
#include <stddef.h>
#include <string.h>
#include <debug.h>
#include <apic.h>
#include <cpu.h>


#define IA32_FS_BASE	0xc0000100

static void *percpu_area[MAX_CPU_NR];

void percpu_cpu_setup(void)
{
	const int apic_id = local_apic_id();

	for (int i = 0; i != local_apics; ++i) {
		if (local_apic_ids[i] != apic_id)
			continue;

		write_msr(IA32_FS_BASE, (uintptr_t)percpu_area[i]);
		return;
	}
	BUG("Failed to find local apic id %d\n", apic_id);
}

void percpu_setup(void)
{
	extern char percpu_phys_begin[];
	extern char percpu_phys_end[];

	const size_t percpu_size = percpu_phys_end - percpu_phys_begin;

	for (int i = 0; i != local_apics; ++i) {
		const uintptr_t addr = balloc_alloc(
					percpu_size + sizeof(uint64_t),
					/* from = */0, /* to = */UINTPTR_MAX);
		BUG_ON(addr == UINTPTR_MAX);

		uint64_t * const baseptr = (uint64_t *)(addr + percpu_size);

		percpu_area[i] = (void *)(addr + percpu_size);
		memcpy((void *)addr, percpu_phys_begin, percpu_size);
		*baseptr = addr + percpu_size;
	}
	percpu_cpu_setup();
}
