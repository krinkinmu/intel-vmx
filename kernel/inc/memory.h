#ifndef __MEMORY_H__
#define __MEMORY_H__

#include <stdint.h>

#define PAGE_SHIFT	12
#define PAGE_SIZE	(1 << PAGE_SHIFT)
#define PAGE_MASK	(PAGE_SIZE - 1)
#define MAX_ORDER	18
#define LOW_MEMORY	(1ull << 32)

#define PA_LOW_MEM	(1ul << 0)

struct page;

uintptr_t page_addr(const struct page *page);

void page_alloc_setup(void);
struct page *__page_alloc(int order, unsigned long flags);
uintptr_t page_alloc(int order, unsigned long flags);
void __page_free(struct page *page, int order);
void page_free(uintptr_t addr, int order);

#endif /*__MEMORY_H__*/
