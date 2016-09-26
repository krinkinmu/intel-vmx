#include <percpu.h>
#include <stddef.h>
#include <string.h>
#include <alloc.h>
#include <debug.h>
#include <cpu.h>


#define IA32_FS_BASE	0xc0000100

void percpu_cpu_setup(void)
{
	extern char percpu_phys_begin[];
	extern char percpu_phys_end[];

	const size_t percpu_size = percpu_phys_end - percpu_phys_begin;
	void *ptr = mem_alloc(percpu_size);

	BUG_ON(!ptr);
	memcpy(ptr, percpu_phys_begin, percpu_size);
	write_msr(IA32_FS_BASE, (uintptr_t)ptr + percpu_size);
}
