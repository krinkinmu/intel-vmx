#include <memory.h>
#include <balloc.h>
#include <debug.h>
#include <list.h>


#define MAX_ORDER	18	/* with 4KB page max allocation is 1GB */

struct page {
	struct list_head link;
};

struct page_alloc_zone {
	struct list_head ll;
	unsigned long long begin;
	unsigned long long end;
	unsigned long long free_pages;
	struct list_head order[MAX_ORDER + 1];
	struct page pages[1];
};

static struct list_head page_alloc_zones;

static void page_alloc_zone_setup(unsigned long long zbegin,
			unsigned long long zend)
{
	static const unsigned long long page_mask =
				~((unsigned long long)PAGE_SIZE - 1);
	const unsigned long long begin = (zbegin + PAGE_SIZE - 1) & page_mask;
	const unsigned long long end = zend & page_mask;
	const unsigned long long pages = (end - begin) / PAGE_SIZE;

	if (!pages)
		return;

	printf("page alloc zone [0x%llx; 0x%llx]\n", begin, end);

	const size_t size = sizeof(struct page_alloc_zone)
				+ sizeof(struct page) * pages;
	struct page_alloc_zone *zone = (struct page_alloc_zone *)
				balloc_alloc(size, 0, UINTPTR_MAX);

	zone->begin = begin;
	zone->end = end;
	zone->free_pages = 0;

	for (int i = 0; i != MAX_ORDER + 1; ++i)
		list_init(&zone->order[i]);
	list_add_tail(&zone->ll, &page_alloc_zones);
}

void page_alloc_setup(void)
{
	struct rb_node *ptr = rb_leftmost(memory_map.root);

	list_init(&page_alloc_zones);

	while (ptr) {
		struct memory_node *node = CONTAINER_OF(ptr, struct memory_node,
					link.ll);

		page_alloc_zone_setup(node->begin, node->end);
		ptr = rb_next(ptr);
	}
}
