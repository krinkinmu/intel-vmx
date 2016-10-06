#ifndef __PAGING_H__
#define __PAGING_H__

#include <spinlock.h>
#include <stdint.h>
#include <rbtree.h>

#define __PTE_PRESENT	((pte_t)1 << 0)
#define __PTE_LARGE	((pte_t)1 << 7)
#define PTE_WRITE	((pte_t)1 << 1)
#define PTE_USER	((pte_t)1 << 2)

#define PTE_PHYS_MASK	((pte_t)0xffffffffff000)
#define PTE_FLAGS_MASK	(~PTE_PHYS_MASK)

#define PT_ENTRIES	512
#define PT_LEVELS	5


typedef uint64_t pte_t;

struct pt_range {
	struct rb_node rb;
	uintptr_t begin;
	uintptr_t end;
	uintptr_t phys;
	pte_t flags;
};

struct page_table {
	struct rb_tree ranges;
	pte_t *pml4;
};

int pt_setup(struct page_table *pt);
void pt_release(struct page_table *pt);
int pt_map(struct page_table *pt, uintptr_t begin, uintptr_t end,
			uintptr_t phys, pte_t pte_flags);
void pt_unmap(struct page_table *pt, uintptr_t begin, uintptr_t end);

void paging_setup(void);
void paging_cpu_setup(void);

#endif /*__PAGING_H__*/
