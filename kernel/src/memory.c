#include <memory.h>
#include <string.h>
#include <balloc.h>
#include <debug.h>

#define PAGE_FREE_OFFS	8
#define PAGE_FREE_MASK	(1ul << PAGE_FREE_OFFS)
#define PAGE_ORDER_MASK	(PAGE_FREE_MASK - 1)
#define PAGE_USER_OFFS	16
#define MEMORY_RANGES	(sizeof(memory_range)/sizeof(memory_range[0]))

struct memory_range {
	uintptr_t begin;
	uintptr_t end;
	unsigned long flags;
};

static const struct memory_range memory_range[] = {
	{0, LOW_MEMORY, PA_LOW},
	{LOW_MEMORY, NORMAL_MEMORY, PA_NORMAL},
	{NORMAL_MEMORY, HIGH_MEMORY, PA_HIGH},
	{HIGH_MEMORY, UNMAPPED_MEMORY, PA_UNMAPPED}
};

struct list_head page_alloc_zones;

static inline int page_order(const struct page *page)
{
	return page->flags & PAGE_ORDER_MASK;
}

static inline void page_set_order(struct page *page, int order)
{
	BUG_ON(order > (int)PAGE_ORDER_MASK);
	BUG_ON(order < 0);

	page->flags = (page->flags & ~PAGE_ORDER_MASK) | order;
}

static inline int page_is_free(const struct page *page)
{
	return (page->flags & PAGE_FREE_MASK) ? 1 : 0;
}

static inline void page_set_free(struct page *page)
{
	page->flags |= PAGE_FREE_MASK;
}

static inline void page_set_busy(struct page *page)
{
	page->flags &= ~PAGE_FREE_MASK;
}

void page_set_bit(struct page *page, int bit)
{
	page->flags |= 1ul << (bit + PAGE_USER_OFFS);
}

void page_clear_bit(struct page *page, int bit)
{
	page->flags &= ~(1ul << (bit + PAGE_USER_OFFS));
}

int page_test_bit(const struct page *page, int bit)
{
	return !!(page->flags & (1ul << (bit + PAGE_USER_OFFS)));
}

static void __page_alloc_zone_setup(uintptr_t zbegin, uintptr_t zend,
			unsigned long flags)
{
	const uintptr_t page_mask = ~((uintptr_t)PAGE_MASK);
	const uintptr_t begin_addr = (zbegin + PAGE_SIZE - 1) & page_mask;
	const uintptr_t end_addr = zend & page_mask;
	const uintptr_t begin = begin_addr >> PAGE_SHIFT;
	const uintptr_t end = end_addr >> PAGE_SHIFT;

	if (begin >= end)
		return;

	const size_t pages = end - begin;
	const size_t size = sizeof(struct page_alloc_zone)
				+ sizeof(struct page) * pages;
	struct page_alloc_zone *zone = (struct page_alloc_zone *)
				__balloc_alloc(size, PAGE_SIZE,
					/* from = */BOOTSTRAP_BEGIN,
					/* to = */BOOTSTRAP_END);

	BUG_ON((uintptr_t)zone == BOOTSTRAP_END);

	printf("page alloc zone [0x%llx; 0x%llx]\n", (unsigned long long)begin,
				(unsigned long long)end);
	memset(zone->pages, 0, sizeof(struct page) * (zone->end - zone->end));
	zone->flags = flags;
	zone->begin = begin;
	zone->end = end;

	for (int i = 0; i != MAX_ORDER + 1; ++i)
		list_init(&zone->order[i]);
	spin_lock_init(&zone->lock);
	list_add_tail(&zone->ll, &page_alloc_zones);
}

static void page_alloc_zone_setup(const struct memory_node *node)
{
	for (int i = 0; i != MEMORY_RANGES; ++i) {
		const struct memory_range *range = &memory_range[i];

		if (node->end <= range->begin || node->begin >= range->end)
			continue;

		const uintptr_t begin = node->begin > range->begin ?
					node->begin : range->begin;
		const uintptr_t end = node->end < range->end ?
					node->end : range->end;

		__page_alloc_zone_setup(begin, end, range->flags);
	}
}

static struct page_alloc_zone *page_alloc_zone_find(uintptr_t idx)
{
	struct list_head *head = &page_alloc_zones;
	struct list_head *ptr;

	for (ptr = head->next; ptr != head; ptr = ptr->next) {
		struct page_alloc_zone *zone = CONTAINER_OF(ptr,
					struct page_alloc_zone, ll);

		if (idx >= zone->begin && idx < zone->end)
			return zone;
	}

	return 0;
}

static struct page_alloc_zone *page_zone(const struct page *page)
{
	struct list_head *head = &page_alloc_zones;
	struct list_head *ptr;

	for (ptr = head->next; ptr != head; ptr = ptr->next) {
		struct page_alloc_zone *zone = CONTAINER_OF(ptr,
					struct page_alloc_zone, ll);
		const size_t pages = zone->end - zone->begin;

		if ((size_t)(page - zone->pages) < pages)
			return zone;
	}

	BUG("orphan page");

	return 0;
}

uintptr_t page_addr(const struct page *page)
{
	const struct page_alloc_zone *zone = page_zone(page);

	return (zone->begin + (page - zone->pages)) << PAGE_SHIFT;
}

struct page *addr_page(uintptr_t addr)
{
	const uintptr_t page = addr >> PAGE_SHIFT;
	struct list_head *head = &page_alloc_zones;
	struct list_head *ptr;

	BUG_ON(addr & PAGE_MASK);

	for (ptr = head->next; ptr != head; ptr = ptr->next) {
		struct page_alloc_zone *zone = CONTAINER_OF(ptr,
					struct page_alloc_zone, ll);
		if (page < zone->begin || page >= zone->end)
			continue;
		return &zone->pages[page - zone->begin];
	}
	BUG("Page for addr 0x%lx not found\n", (unsigned long)addr);
	return 0;
}

static void __page_alloc_zone_free(uintptr_t zbegin, uintptr_t zend)
{
	const uintptr_t page_mask = ~((uintptr_t)PAGE_MASK);
	const uintptr_t begin_addr = (zbegin + PAGE_SIZE - 1) & page_mask;
	const uintptr_t end_addr = zend & page_mask;
	const uintptr_t begin = begin_addr ? begin_addr >> PAGE_SHIFT : 1;
	const uintptr_t end = end_addr >> PAGE_SHIFT;

	if (begin >= end)
		return;

	struct page_alloc_zone *zone = page_alloc_zone_find(begin);

	BUG_ON(!zone);
	BUG_ON(begin < zone->begin || begin >= zone->end);
	BUG_ON(end <= zone->begin || end > zone->end);

	for (uintptr_t page = begin; page != end;) {
		int order;

		for (order = 0; order < MAX_ORDER; ++order) {
			if (page & (1ull << order))
				break;
			if (page + (1ull << (order + 1)) > end)
				break;
		}

		const size_t pages = (size_t)1 << order;
		struct page *ptr = &zone->pages[page - zone->begin];

		list_add_tail(&ptr->ll, &zone->order[order]);
		page_set_order(ptr, order);
		page_set_free(ptr);
		page += pages;
	}
}

static void page_alloc_zone_free(const struct memory_node *node)
{
	for (int i = 0; i != MEMORY_RANGES; ++i) {
		const struct memory_range *range = &memory_range[i];

		if (node->end <= range->begin || node->begin >= range->end)
			continue;

		const uintptr_t begin = node->begin > range->begin ?
					node->begin : range->begin;
		const uintptr_t end = node->end < range->end ?
					node->end : range->end;

		__page_alloc_zone_free(begin, end);
	}
}

static void page_alloc_zone_dump(const struct page_alloc_zone *zone)
{
	printf("zone 0x%llx-0x%llx:\n",
				(unsigned long long)(zone->begin << PAGE_SHIFT),
				(unsigned long long)(zone->end << PAGE_SHIFT));
	for (int order = MAX_ORDER; order >= 0; --order) {
		if (list_empty(&zone->order[order]))
			continue;

		const unsigned long count = list_size(&zone->order[order]);
		const unsigned long block_size = (1ul << order) << PAGE_SHIFT;

		printf("    %lu blocks of size %lu\n", count, block_size);
	}
}

void page_alloc_setup(void)
{
	struct rb_node *ptr = rb_leftmost(memory_map.root);

	list_init(&page_alloc_zones);

	while (ptr) {
		const struct memory_node *node = RB2MEMORY_NODE(ptr);

		page_alloc_zone_setup(node);
		ptr = rb_next(ptr);
	}

	ptr = rb_leftmost(free_ranges.root);

	while (ptr) {
		const struct memory_node *node = RB2MEMORY_NODE(ptr);

		page_alloc_zone_free(node);
		ptr = rb_next(ptr);
	}

	struct list_head *head = &page_alloc_zones;

	for (struct list_head *p = head->next; p != head; p = p->next) {
		struct page_alloc_zone *zone = CONTAINER_OF(p,
					struct page_alloc_zone, ll);

		page_alloc_zone_dump(zone);
	}
}

static struct page *__page_alloc_zone(struct page_alloc_zone *zone, int order)
{
	int current = order;

	while (list_empty(&zone->order[current]) && current <= MAX_ORDER)
		++current;

	if (current > MAX_ORDER)
		return 0;

	BUG_ON(list_empty(&zone->order[current]));

	struct page *page = LIST_ENTRY(list_first(&zone->order[current]),
				struct page, ll);
	const uintptr_t idx = zone->begin + (page - zone->pages);

	page_set_busy(page);
	list_del(&page->ll);

	while (current != order) {
		const uintptr_t bidx = idx ^ (1ull << --current);
		struct page *buddy = zone->pages + (bidx - zone->begin);

		list_add(&buddy->ll, &zone->order[current]);

		page_set_order(buddy, current);
		page_set_free(buddy);
	}

	return page;
}

static struct page *page_alloc_zone(struct page_alloc_zone *zone, int order)
{
	const unsigned long flags = spin_lock_save(&zone->lock);
	struct page *page = __page_alloc_zone(zone, order);

	spin_unlock_restore(&zone->lock, flags);
	return page;
}

struct page *__page_alloc(int order, unsigned long flags)
{
	if (order > MAX_ORDER)
		return 0;

	struct list_head *head = &page_alloc_zones;
	struct list_head *ptr;

	for (ptr = head->next; ptr != head; ptr = ptr->next) {
		struct page_alloc_zone *zone = CONTAINER_OF(ptr,
					struct page_alloc_zone, ll);

		if ((zone->flags & flags) == 0ul)
			continue;

		struct page *page = page_alloc_zone(zone, order);

		if (page)
			return page;
	}

	return 0;
}

uintptr_t page_alloc(int order, unsigned long flags)
{
	if (order > MAX_ORDER)
		return 0;

	struct list_head *head = &page_alloc_zones;
	struct list_head *ptr;

	for (ptr = head->next; ptr != head; ptr = ptr->next) {
		struct page_alloc_zone *zone = CONTAINER_OF(ptr,
					struct page_alloc_zone, ll);

		if ((zone->flags & flags) == 0ul)
			continue;

		struct page *page = page_alloc_zone(zone, order);

		if (!page)
			continue;

		const uintptr_t index = zone->begin + (page - zone->pages);

		return index << PAGE_SHIFT;
	}

	return 0;
}

static void __page_free_zone(struct page_alloc_zone *zone, struct page *page,
			int order)
{
	uintptr_t idx = zone->begin + (page - zone->pages);

	while (order < MAX_ORDER) {
		const uintptr_t bidx = idx ^ (1ull << order);

		if (bidx < zone->begin || bidx >= zone->end)
			break;

		struct page *buddy = &zone->pages[bidx - zone->begin];

		if (!page_is_free(buddy) || page_order(buddy) != order)
			break;

		if (bidx < idx) {
			page = buddy;
			idx = bidx;
		}

		list_del(&buddy->ll);
		++order;
	}

	list_add(&page->ll, &zone->order[order]);

	page_set_order(page, order);
	page_set_free(page);
}

static void page_free_zone(struct page_alloc_zone *zone, struct page *page,
			int order)
{
	const unsigned long flags = spin_lock_save(&zone->lock);

	__page_free_zone(zone, page, order);
	spin_unlock_restore(&zone->lock, flags);
}

void page_free(uintptr_t addr, int order)
{
	if (!addr)
		return;

	const uintptr_t idx = addr >> PAGE_SHIFT;
	struct page_alloc_zone *zone = page_alloc_zone_find(idx);

	BUG_ON(!zone);

	struct page *page = &zone->pages[idx - zone->begin];

	page_free_zone(zone, page, order);
}

void __page_free(struct page *page, int order)
{
	if (!page)
		return;

	struct page_alloc_zone *zone = page_zone(page);

	page_free_zone(zone, page, order);
}

uintptr_t phys_mem_limit(void)
{
	uintptr_t limit = 0;

	struct list_head *head = &page_alloc_zones;
	struct list_head *ptr;

	for (ptr = head->next; ptr != head; ptr = ptr->next) {
		struct page_alloc_zone *zone = CONTAINER_OF(ptr,
					struct page_alloc_zone, ll);
		const uintptr_t end = zone->end << PAGE_SHIFT;

		if (end > limit)
			limit = end;
	}

	return limit;
}
