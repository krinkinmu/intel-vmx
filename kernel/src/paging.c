#include <paging.h>
#include <stdint.h>
#include <memory.h>
#include <string.h>
#include <debug.h>

#define PTE_PRESENT	((uintptr_t)1 << 0)
#define PTE_WRITE	((uintptr_t)1 << 1)
#define PTE_USER	((uintptr_t)1 << 2)
#define PTE_LARGE	((uintptr_t)1 << 7)

#define PTE_PHYS_MASK	((uintptr_t)0xffffffffff000)
#define PTE_FLAGS_MASK	(~PTE_PHYS_MASK)

typedef uint64_t	pte_t;

static int pml_offs(uintptr_t addr, int level)
{
	static const int offs[] = {0, 12, 21, 30, 39};
	static const int mask[] = {0, 0xfff, 0x1ff, 0x1ff, 0x1ff};

	BUG_ON(level < 1 || level > 4);
	return (addr >> offs[level]) & mask[level];
}

static uintptr_t pml_size(int level)
{
	static const int offs[] = {0, 12, 21, 30, 39};

	BUG_ON(level < 1 || level > 4);
	return (uintptr_t)1 << offs[level];
}

static uintptr_t pte_phys(pte_t pte)
{
	return pte & PTE_PHYS_MASK;
}

static uintptr_t page_table_alloc(void)
{
	uintptr_t addr = page_alloc(0, PA_NORMAL);

	if (!addr)
		return 0;

	memset((void *)addr, 0, PAGE_SIZE);
	return addr;
}

static void __pml_map(pte_t *pml, uintptr_t begin, uintptr_t end,
			uintptr_t phys, uintptr_t flags, int level)
{
	const uintptr_t size = pml_size(level);
	const uintptr_t mask = size - 1;
	const uintptr_t large = (level == 3 || level == 2) ? PTE_LARGE : 0;
	const int start = pml_offs(begin, level);
	const int finish = pml_offs(end - 1, level);

	for (int i = start; i <= finish; ++i) {
		const uintptr_t entry_end = (begin + size) & ~mask;
		const uintptr_t tomap = entry_end < end
					? entry_end - begin
					: end - begin;

		if (!(pml[i] & PTE_PRESENT)) {
			if (tomap == size) {
				pml[i] = phys | flags | large;
				begin += tomap;
				phys += tomap;
				continue;
			}

			BUG_ON(level == 1);

			pml[i] = page_table_alloc() | flags;
		}

		pte_t *next = (pte_t *)pte_phys(pml[i]);

		BUG_ON(!next);
		if (level != 1 && (level == 4 || !(pml[i] & PTE_LARGE)))
			__pml_map(next, begin, begin + tomap, phys,
						flags, level - 1);
		begin += tomap;
		phys += tomap;
	}
}

static void pml_map(pte_t *pml4, uintptr_t begin, uintptr_t end,
			uintptr_t phys, uintptr_t flags)
{
	BUG_ON(begin & (PAGE_SIZE - 1));
	BUG_ON(end & (PAGE_SIZE - 1));
	BUG_ON(phys & (PAGE_SIZE - 1));

	__pml_map(pml4, begin, end, phys, flags, 4);
}

void paging_early_setup(void)
{
	uintptr_t mem_limit = phys_mem_limit();

	if (mem_limit > HIGH_MEMORY)
		mem_limit = HIGH_MEMORY;

	pte_t *old_pml4 = (pte_t *)read_cr3();
	pte_t *new_pml4 = (pte_t *)page_table_alloc();

	BUG_ON(!new_pml4);
	pml_map(new_pml4, 0, mem_limit, 0,
				PTE_PRESENT | PTE_WRITE);
	write_cr3((uintptr_t)new_pml4);
	(void) old_pml4;
}

void paging_setup(void)
{
	// TODO: refine page table, unmap regions that don't exists, unmap zero page,
	// forbid execution from data pages, forbid write to code/rodata pages.
}
