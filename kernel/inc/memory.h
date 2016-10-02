#ifndef __MEMORY_H__
#define __MEMORY_H__

#include <stdint.h>

#define PAGE_SHIFT	12
#define PAGE_SIZE	(1 << PAGE_SHIFT)
#define PAGE_MASK	(PAGE_SIZE - 1)
#define MAX_ORDER	18

#define UNMAPPED_MEMORY	UINTPTR_MAX
#define HIGH_MEMORY	(1ull << 47)
#define NORMAL_MEMORY	(1ull << 32)
#define LOW_MEMORY	(1ull << 20)

#define PA_LOW		(1ul << 0)
#define PA_NORMAL	(1ul << 1)
#define PA_HIGH		(1ul << 2)
#define PA_UNMAPPED	(1ul << 3)
#define PA_ANY		(PA_LOW | PA_NORMAL | PA_HIGH)

#define KERNEL_CS	0x08

struct page;

uintptr_t page_addr(const struct page *page);

void page_alloc_setup(void);
struct page *__page_alloc(int order, unsigned long flags);
uintptr_t page_alloc(int order, unsigned long flags);
void __page_free(struct page *page, int order);
void page_free(uintptr_t addr, int order);
uintptr_t phys_mem_limit(void);

#endif /*__MEMORY_H__*/
