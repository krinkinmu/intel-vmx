#ifndef __PAGING_H__
#define __PAGING_H__

#include <stdint.h>

#define PTE_PRESENT	((pte_t)1 << 0)
#define PTE_WRITE	((pte_t)1 << 1)
#define PTE_USER	((pte_t)1 << 2)
#define PTE_LARGE	((pte_t)1 << 7)

#define PTE_PHYS_MASK	((pte_t)0xffffffffff000)
#define PTE_FLAGS_MASK	(~PTE_PHYS_MASK)

#define PT_ENTRIES	512
#define PT_LEVELS	5


typedef uint64_t pte_t;

struct pt_iter {
	pte_t *table[PT_LEVELS];
	int index[PT_LEVELS];
	int lvl;
};

void pt_iter_setup(pte_t *pml4, struct pt_iter *iter, uintptr_t addr);
void pt_iter_next_slot(struct pt_iter *iter);
void pt_iter_prev_slot(struct pt_iter *iter);
uintptr_t pt_iter_slot_addr(const struct pt_iter *iter);
uintptr_t pt_iter_slot_size(const struct pt_iter *iter);

void paging_early_setup(void);
void paging_setup(void);

#endif /*__PAGING_H__*/
