#include <paging.h>
#include <stdint.h>
#include <memory.h>
#include <string.h>
#include <debug.h>

static int pml_shift(int level)
{
	static const int offs[] = {0, 12, 21, 30, 39};

	BUG_ON(level < 1 || level > PT_LEVELS - 1);
	return offs[level];
}

static int pml_offs(uintptr_t addr, int level)
{
	static const int mask[] = {0, 0xfff, 0x1ff, 0x1ff, 0x1ff};

	BUG_ON(level < 1 || level > PT_LEVELS - 1);
	return (addr >> pml_shift(level)) & mask[level];
}

static uintptr_t pml_size(int level)
{
	BUG_ON(level < 1 || level > PT_LEVELS - 1);
	return (uintptr_t)1 << pml_shift(level);
}

void pt_iter_setup(pte_t *pml4, struct pt_iter *iter, uintptr_t addr)
{
	iter->lvl = PT_LEVELS - 1;
	iter->table[iter->lvl] = pml4;
	iter->index[iter->lvl] = pml_offs(addr, iter->lvl);

	for (int i = PT_LEVELS - 1; i != 1; --i) {
		const int index = iter->index[i];
		const pte_t pte = iter->table[i][index];

		if (!(pte & PTE_PRESENT) || (pte & PTE_LARGE))
			break;

		iter->table[--iter->lvl] = (pte_t *)(pte & PTE_PHYS_MASK);
		iter->index[iter->lvl] = pml_offs(addr, iter->lvl);
	}
}

void pt_iter_next_slot(struct pt_iter *iter)
{
	for (int i = iter->lvl; i != PT_LEVELS; ++i) {
		if (++iter->index[i] != PT_ENTRIES)
			break;
		++iter->lvl;
	}

	if (iter->lvl == PT_LEVELS)
		iter->index[--iter->lvl] = 0;

	for (int i = iter->lvl; i != 1; --i) {
		const int index = iter->index[i];
		const pte_t pte = iter->table[i][index];

		if (!(pte & PTE_PRESENT) || (pte & PTE_LARGE))
			break;

		iter->table[--iter->lvl] = (pte_t *)(pte & PTE_PHYS_MASK);
		iter->index[iter->lvl] = 0;
	}
}

void pt_iter_prev_slot(struct pt_iter *iter)
{
	for (int i = iter->lvl; i != PT_LEVELS; ++i) {
		if (--iter->index[i] != -1)
			break;
		++iter->lvl;
	}

	if (iter->lvl == PT_LEVELS)
		iter->index[--iter->lvl] = PT_ENTRIES - 1;

	for (int i = iter->lvl; i != 1; --i) {
		const int index = iter->index[i];
		const pte_t pte = iter->table[i][index];

		if (!(pte & PTE_PRESENT) || (pte & PTE_LARGE))
			break;

		iter->table[--iter->lvl] = (pte_t *)(pte & PTE_PHYS_MASK);
		iter->index[iter->lvl] = PT_ENTRIES - 1;
	}
}

uintptr_t pt_iter_slot_size(const struct pt_iter *iter)
{
	return pml_size(iter->lvl);
}

uintptr_t pt_iter_slot_addr(const struct pt_iter *iter)
{
	uintptr_t addr = 0;

	for (int i = iter->lvl; i != PT_LEVELS; ++i)
		addr |= (uintptr_t)iter->index[i] << pml_shift(i);

	if (addr & ((uintptr_t)1 << 47))
		addr |= 0xffff000000000000ull;

	return addr;
}

void paging_early_setup(void)
{
}

void paging_setup(void)
{
}
