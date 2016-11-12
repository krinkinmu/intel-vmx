#include <paging.h>
#include <stdint.h>
#include <memory.h>
#include <string.h>
#include <debug.h>
#include <alloc.h>
#include <list.h>


static struct mem_cache pt_range_cache;
static struct page_table kernel_pt;


static int pml_shift(int level)
{
	static const int offs[] = {0, 12, 21, 30, 39};

	BUG_ON(level < 1 || level > 4);
	return offs[level];
}

static int pml_offs(uintptr_t addr, int level)
{
	static const int mask[] = {0, 0xfff, 0x1ff, 0x1ff, 0x1ff};

	BUG_ON(level < 1 || level > 4);
	return (addr >> pml_shift(level)) & mask[level];
}

static uintptr_t pml_size(int level)
{
	BUG_ON(level < 1 || level > 4);
	return (uintptr_t)1 << pml_shift(level);
}

static pte_t *pt_alloc(unsigned long flags)
{
	pte_t *pte = (void *)page_alloc(0, flags);

	if (!pte)
		return 0;

	memset(pte, 0, PAGE_SIZE);
	return pte;
}

static void pt_free(pte_t *pt)
{
	page_free((uintptr_t)pt, 0);
}

static struct pt_range *pt_alloc_range(unsigned long flags)
{
	return mem_cache_alloc(&pt_range_cache, flags);
}

static void pt_free_range(struct pt_range *range)
{
	mem_cache_free(&pt_range_cache, range);
}

static struct pt_range *pt_find_prev_range(struct page_table *pt,
			uintptr_t addr)
{
	struct rb_node *node = pt->ranges.root;
	struct pt_range *lower = 0;

	while (node) {
		struct pt_range *range = TREE_ENTRY(node, struct pt_range, rb);

		if (range->end >= addr) {
			node = node->left;
			lower = range;
		} else {
			node = node->right;
		}
	}

	return lower;
}

static struct pt_range *pt_find_next_range(struct page_table *pt,
			uintptr_t addr)
{
	struct rb_node *node = pt->ranges.root;
	struct pt_range *upper = 0;

	while (node) {
		struct pt_range *range = TREE_ENTRY(node, struct pt_range, rb);

		if (range->begin <= addr) {
			node = node->right;
			upper = range;
		} else {
			node = node->left;
		}
	}

	return upper;
}

static void pt_insert_range(struct page_table *pt, struct pt_range *new)
{
	struct rb_node **plink = &pt->ranges.root;
	struct rb_node *parent = 0;

	while (*plink) {
		struct pt_range *old = TREE_ENTRY(*plink, struct pt_range, rb);

		parent = *plink;
		if (old->begin > new->begin)
			plink = &parent->left;
		else
			plink = &parent->right;
	}

	rb_link(&new->rb, parent, plink);
	rb_insert(&new->rb, &pt->ranges);
}

static void pt_remove_range(struct page_table *pt, struct pt_range *range)
{
	rb_erase(&range->rb, &pt->ranges);
}

static int __pt_count_pages(uintptr_t begin, uintptr_t end, uintptr_t phys, int lvl)
{
	const uintptr_t size = pml_size(lvl);
	const uintptr_t mask = size - 1;

	const int from = pml_offs(begin, lvl);
	const int to = pml_offs(end - 1, lvl) + 1;
	const int notaligned = (begin & mask) || (phys & mask);

	int count = 1;

	if (lvl == 1)
		return count;

	for (int i = from; i != to; ++i) {
		const uintptr_t entry_end = (begin + size) & ~mask;
		const uintptr_t map_begin = begin;
		const uintptr_t map_end = entry_end < end ? entry_end : end;
		const uintptr_t tomap = map_end - map_begin;

		if (lvl == 4 || notaligned)
			count += __pt_count_pages(map_begin, map_end, phys,
						lvl - 1);

		begin += tomap;
		phys += tomap;
	}

	return count;
}

static int pt_count_pages(uintptr_t begin, uintptr_t end, uintptr_t phys)
{
	return __pt_count_pages(begin, end, phys, 4);
}

static void pt_free_pages(const struct list_head *pages)
{
	const struct list_head *head = pages;
	const struct list_head *ptr = head->next;

	while (ptr != head) {
		struct page *page = LIST_ENTRY(ptr, struct page, ll);

		ptr = ptr->next;
		__page_free(page, 0);
	}
}

static int pt_alloc_pages(struct list_head *pages, int count, unsigned long pa_flags)
{
	for (int i = 0; i != count; ++i) {
		struct page *page = __page_alloc(0, pa_flags);

		if (!page) {
			pt_free_pages(pages);
			return -1;
		}
		list_add_tail(&page->ll, pages);
	}

	return 0;
}

static void __pt_map_pages(pte_t *pml, uintptr_t begin, uintptr_t end,
			uintptr_t phys, pte_t flags,
			int lvl, struct list_head *pages)
{
	const pte_t pde_flags = __PTE_PRESENT | PTE_WRITE | PTE_USER;
	const uintptr_t size = pml_size(lvl);
	const uintptr_t mask = size - 1;

	const int from = pml_offs(begin, lvl);
	const int to = pml_offs(end - 1, lvl) + 1;
	const pte_t pte_large = (lvl == 3 || lvl == 2) ? __PTE_LARGE : 0;

	for (int i = from; i != to; ++i) {
		const int notaligned = (begin & mask) || (phys & mask);
		const uintptr_t entry_end = (begin + size) & ~mask;
		const uintptr_t map_end = entry_end < end ? entry_end : end;
		const uintptr_t map_begin = begin;
		const uintptr_t tomap = map_end - map_begin;

		pte_t pte = pml[i];

		if (!(pte & __PTE_PRESENT)) {
			if (lvl != 4 && tomap == size && !notaligned) {
				pml[i] = (pte_t)phys | flags | pte_large;
				begin += tomap;
				phys += tomap;
				continue;
			}
			BUG_ON(lvl == 1);
			BUG_ON(list_empty(pages));

			struct list_head *ll = pages->next;
			struct page *page = LIST_ENTRY(ll, struct page, ll);

			list_del(ll);
			pml[i] = (pte_t)page_addr(page) | pde_flags;
			pte = pml[i];
		}

		BUG_ON(lvl == 1);
		__pt_map_pages((pte_t *)(pte & PTE_PHYS_MASK),
					map_begin, map_end,
					phys, flags, lvl - 1, pages);
		begin += tomap;
		phys += tomap;
	}
}

static void pt_map_pages(struct page_table *pt, uintptr_t begin, uintptr_t end,
			uintptr_t phys, pte_t flags, struct list_head *pages)
{
	__pt_map_pages(pt->pml4, begin, end, phys, flags | __PTE_PRESENT,
				4, pages);
}

static int __pt_map(struct page_table *pt, uintptr_t begin, uintptr_t end,
			uintptr_t phys, pte_t pte_flags, unsigned long pa_flags)
{
	const int count = pt_count_pages(begin, end, phys);
	struct pt_range *range = pt_alloc_range(pa_flags);
	struct list_head pages;

	if (!range)
		return -1;

	range->begin = begin;
	range->end = end;
	range->phys = phys;
	range->flags = pte_flags;

	list_init(&pages);

	if (pt_alloc_pages(&pages, count, pa_flags)) {
		pt_free_range(range);
		return -1;
	}

	pt_map_pages(pt, begin, end, phys, pte_flags, &pages);
	pt_insert_range(pt, range);
	pt_free_pages(&pages);
	return 0;
}

static void __pt_unmap_pages(pte_t *pml, uintptr_t begin, uintptr_t end,
			int lvl)
{
	const uintptr_t size = pml_size(lvl);
	const uintptr_t mask = size - 1;

	const int from = pml_offs(begin, lvl);
	const int to = pml_offs(end - 1, lvl) + 1;

	for (int i = from; i != to; ++i) {
		const pte_t pte = pml[i];
		const uintptr_t entry_end = (begin + size) & ~mask;
		const uintptr_t unmap_end = entry_end < end ? entry_end : end;
		const uintptr_t unmap_begin = begin;
		const uintptr_t tounmap = unmap_end - unmap_begin;

		if (!(pte & __PTE_PRESENT)) {
			begin += tounmap;
			continue;
		}

		if ((lvl == 4) || (lvl > 1 && !(pte & __PTE_LARGE))) {
			const uintptr_t addr = pte & PTE_PHYS_MASK;

			__pt_unmap_pages((pte_t *)addr, unmap_begin, unmap_end,
						lvl - 1);

			if (tounmap == size)
				page_free(addr, 0);
		}

		if (tounmap == size)
			pml[i] = 0;

		begin += tounmap;
	}
}

static void pt_unmap_pages(struct page_table *pt, uintptr_t begin,
			uintptr_t end)
{
	__pt_unmap_pages(pt->pml4, begin, end, 4);
}

static void __pt_unmap(struct page_table *pt, uintptr_t begin, uintptr_t end)
{
	struct pt_range *prev = pt_find_prev_range(pt, begin);
	struct pt_range *next = pt_find_next_range(pt, end);

	if (!prev) {
		struct rb_node *node = rb_leftmost(&pt->ranges);

		begin = 0;
		prev = node ? TREE_ENTRY(node, struct pt_range, rb) : 0;
	}

	if (!next)
		end = HIGH_MEMORY;

	while (prev && prev->begin < end) {
		struct pt_range *range = prev;
		struct rb_node *node = rb_next(&range->rb);

		prev = node ? TREE_ENTRY(node, struct pt_range, rb) : 0;
		if (prev->end <= begin)
			continue;

		pt_remove_range(pt, range);
		pt_free_range(range);
	}
	pt_unmap_pages(pt, begin, end);
}

static void __pt_release(pte_t *pml, int lvl)
{
	for (int i = 0; i != PT_ENTRIES; ++i) {
		const pte_t pte = pml[i];

		if (!(pte & __PTE_PRESENT) || (pte & __PTE_LARGE) || lvl == 1)
			continue;

		__pt_release((pte_t *)(pte & PTE_PHYS_MASK), lvl - 1);
	}
	pt_free(pml);
}

static int __pt_setup(struct page_table *pt, unsigned long flags)
{
	pt->pml4 = pt_alloc(flags);
	pt->ranges.root = 0;

	return pt->pml4 ? 0 : -1;
}

int pt_setup(struct page_table *pt)
{
	return __pt_setup(pt, PA_ANY);
}

void pt_release(struct page_table *pt)
{
	struct rb_node *node = rb_leftmost(&pt->ranges);

	while (node) {
		struct pt_range *range = TREE_ENTRY(node, struct pt_range, rb);

		node = rb_next(node);
		pt_free_range(range);
	}

	__pt_release(pt->pml4, 4);
	pt->ranges.root = 0;
	pt->pml4 = 0;
}

int pt_map(struct page_table *pt, uintptr_t begin, uintptr_t end,
			uintptr_t phys, pte_t pte_flags)
{
	return __pt_map(pt, begin, end, phys, pte_flags, PA_ANY);
}

void pt_unmap(struct page_table *pt, uintptr_t begin, uintptr_t end)
{
	__pt_unmap(pt, begin, end);
}

void paging_setup(void)
{
	const size_t size = sizeof(struct pt_range);
	const uintptr_t mem_limit = phys_mem_limit();

	mem_cache_setup(&pt_range_cache, size, size);
	BUG_ON(__pt_setup(&kernel_pt, PA_NORMAL));
	BUG_ON(__pt_map(&kernel_pt, PAGE_SIZE, mem_limit, PAGE_SIZE,
				PTE_WRITE, PA_NORMAL));
}

void paging_cpu_setup(void)
{
	write_cr3((uintptr_t)kernel_pt.pml4);
}
