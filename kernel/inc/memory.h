#ifndef __MEMORY_H__
#define __MEMORY_H__

#include <stdint.h>

#define PAGE_SHIFT	12
#define PAGE_SIZE	(1 << PAGE_SHIFT)
#define PAGE_MASK	(PAGE_SIZE - 1)
#define MAX_ORDER	18

struct page;

uintptr_t page_addr(const struct page *page);

void page_alloc_setup(void);
struct page *__page_alloc(int order);
uintptr_t page_alloc(int order);
void __page_free(struct page *page, int order);
void page_free(uintptr_t addr, int order);

#endif /*__MEMORY_H__*/
