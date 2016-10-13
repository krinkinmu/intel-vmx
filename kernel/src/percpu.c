#include <percpu.h>
#include <memory.h>
#include <stddef.h>
#include <string.h>
#include <debug.h>
#include <cpu.h>


#define IA32_FS_BASE	0xc0000100

static int alloc_order(size_t size)
{
	int order = 0;
	while (((size_t)1 << (PAGE_SHIFT + order)) < size)
		++order;
	return order;
}

void percpu_cpu_setup(void)
{
	extern char percpu_phys_begin[];
	extern char percpu_phys_end[];

	const size_t percpu_size = percpu_phys_end - percpu_phys_begin;
	const int order = alloc_order(percpu_size + sizeof(uint64_t));
	void *ptr = (void *)page_alloc(order, PA_ANY);

	BUG_ON(!ptr);

	uint64_t *baseptr = (uint64_t *)((uintptr_t)ptr + percpu_size);
	uintptr_t base = (uintptr_t)ptr + percpu_size;

	memcpy(ptr, percpu_phys_begin, percpu_size);
	write_msr(IA32_FS_BASE, base);
	*baseptr = base;
}
