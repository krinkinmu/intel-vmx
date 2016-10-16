#include <limits.h>
#include <memory.h>
#include <string.h>
#include <alloc.h>
#include <debug.h>
#include <rbtree.h>


#define WORD_SZ		sizeof(unsigned long long)
#define OBJS_PER_WORD	(WORD_SZ * CHAR_BIT)
#define MIN_POOL_OBJS	8
#define PAGE_CACHE_BIT	0


struct mem_pool {
	struct list_head ll;
	struct page *page;
	void *data;
	size_t free;
	unsigned long long bitmask[1];
};

struct alignment_off {
	char padding;
	struct mem_pool pool;
};

static int ilog2(uintmax_t x)
{
	uintmax_t val = 1;
	int power = 0;

	while (val < x) {
		val <<= 1;
		++power;
	}
	return power;
}

static uintmax_t ceil(uintmax_t divident, uintmax_t divisor)
{
	return (divident + divisor - 1) / divisor;
}

static uintmax_t floor(uintmax_t divident, uintmax_t divisor)
{
	return divident / divisor;
}

static uintmax_t align_up(uintmax_t size, uintmax_t align)
{
	/* NOTE: usually align is a power of 2, so we can achieve the
	 * same effect without expensive division and multiplication,
	 * hope compiler can inline the function and deduce that. */
	return align * ceil(size, align);
}

static uintmax_t align_down(uintmax_t size, uintmax_t align)
{
	return align * floor(size, align);
} 

static size_t meta_size(size_t objs)
{
	static const size_t meta_align = offsetof(struct alignment_off, pool);

	const size_t bitmask_size = (ceil(objs, OBJS_PER_WORD) - 1) * WORD_SZ;

	return align_up(sizeof(struct mem_pool) + bitmask_size, meta_align);
}

static void mem_cache_layout_setup(struct mem_cache *cache,
			size_t size, size_t align)
{
	const size_t obj_size = align_up(size, align);
	const size_t min_size = obj_size * MIN_POOL_OBJS +
				meta_size(MIN_POOL_OBJS);
	const int pool_order = ilog2(min_size >> PAGE_SHIFT);
	const size_t pool_size = (size_t)1 << (pool_order + PAGE_SHIFT);

	size_t objs = pool_size / obj_size;

	while (obj_size * objs + meta_size(objs) > pool_size)
		--objs;

	cache->meta_offs = pool_size - meta_size(objs);
	cache->obj_count = objs;
	cache->mask_words = ceil(objs, OBJS_PER_WORD);
	cache->obj_size = obj_size;
	cache->pool_order = pool_order;
}

static void mem_pool_bitmap_setup(struct mem_cache *cache,
			struct mem_pool *pool)
{
	const size_t words = cache->mask_words;
	const size_t pos = cache->obj_count % OBJS_PER_WORD;
	const size_t word = cache->obj_count / OBJS_PER_WORD;

	memset(pool->bitmask, 0, sizeof(pool->bitmask[0]) * words);

	if (pos)
		pool->bitmask[word] = ~((1ul << pos) - 1);

	for (size_t w = word + 1; w < words; ++w)
		pool->bitmask[w] = ~0ul;
}

static struct mem_pool *mem_pool_create(struct mem_cache *cache,
			unsigned long flags)
{
	struct page *page = __page_alloc(cache->pool_order, flags);

	if (!page)
		return 0;

	const uintptr_t addr = page_addr(page); 
	struct mem_pool *meta = (struct mem_pool *)(addr + cache->meta_offs);

	for (int i = 0; i != 1 << cache->pool_order; ++i) {
		page_set_bit(&page[i], PAGE_CACHE_BIT);
		page[i].u.cache = cache;
	}

	meta->page = page;
	meta->data = (void *)addr;
	meta->free = cache->obj_count;
	mem_pool_bitmap_setup(cache, meta);

	return meta;
}

static void mem_pool_destroy(struct mem_cache *cache, struct mem_pool *pool)
{
	BUG_ON(pool->free != cache->obj_count);

	struct page *page = pool->page;

	for (int i = 0; i != 1 << cache->pool_order; ++i) {
		page_clear_bit(&page[i], PAGE_CACHE_BIT);
		page[i].u.cache = 0;
	}

	page_free((uintptr_t)pool->data, cache->pool_order);
}

void mem_cache_setup(struct mem_cache *cache, size_t size, size_t align)
{
	mem_cache_layout_setup(cache, size, align);

	spin_lock_init(&cache->lock);
	list_init(&cache->free_pools);
	list_init(&cache->partial_pools);
	list_init(&cache->busy_pools);
}

void mem_cache_release(struct mem_cache *cache)
{
	BUG_ON(!list_empty(&cache->busy_pools));
	BUG_ON(!list_empty(&cache->partial_pools));

	struct list_head *head = &cache->free_pools;

	for (struct list_head *ptr = head->next; ptr != head;) {
		struct mem_pool *pool = LIST_ENTRY(ptr, struct mem_pool, ll);

		ptr = ptr->next;
		mem_pool_destroy(cache, pool);
	}
}

static int ffs(unsigned long long bitmask)
{
	unsigned long long mask = 0xffull;
	int byte, bit;

	for (byte = 0; byte != 8; ++byte, mask <<= 8)
		if (bitmask & mask)
			break;

	mask = 0x01ull << (byte * 8);
	for (bit = 0; bit != 8; ++bit, mask <<= 1)
		if (bitmask & mask)
			break;

	return byte * 8 + bit;
}

static void *mem_pool_alloc(struct mem_cache *cache, struct mem_pool *pool)
{
	BUG_ON(!pool->free);

	const size_t words = cache->mask_words;

	for (size_t word = 0; word != words; ++word) {
		if (pool->bitmask[word] == ~0ul)
			continue;

		const size_t bit = ffs(~pool->bitmask[word]);
		const size_t pos = OBJS_PER_WORD * word + bit;
		const uintptr_t addr = (uintptr_t)pool->data;

		pool->bitmask[word] |= (1ul << bit);
		--pool->free;
		return (void *)(addr + pos * cache->obj_size);
	}
	BUG("Failed to find free slot in mem_pool")
}

static void mem_pool_free(struct mem_cache *cache, struct mem_pool *pool,
			void *ptr)
{
	const uintptr_t offs = (uintptr_t)ptr - (uintptr_t)pool->data;
	const size_t pos = offs / cache->obj_size;

	BUG_ON(pos >= cache->obj_count);

	size_t word = pos / OBJS_PER_WORD;
	size_t bit = pos % OBJS_PER_WORD;

	BUG_ON(!(pool->bitmask[word] & (1ull << bit)));

	pool->bitmask[word] &= ~(1ull << bit);
	++pool->free;

	BUG_ON(pool->free > cache->obj_count);
}

static void *__mem_cache_alloc(struct mem_cache *cache, unsigned long flags)
{
	if (!list_empty(&cache->partial_pools)) {
		struct list_head *ptr = list_first(&cache->partial_pools);
		struct mem_pool *pool = LIST_ENTRY(ptr, struct mem_pool, ll);

		void *data = mem_pool_alloc(cache, pool);

		if (!pool->free) {
			list_del(&pool->ll);
			list_add(&pool->ll, &cache->busy_pools);
		}
		return data;
	}

	if (!list_empty(&cache->free_pools)) {
		struct list_head *ptr = list_first(&cache->free_pools);
		struct mem_pool *pool = LIST_ENTRY(ptr, struct mem_pool, ll);

		void *data = mem_pool_alloc(cache, pool);

		if (pool->free != cache->obj_count) {
			list_del(&pool->ll);
			list_add(&pool->ll, &cache->partial_pools);
		}
		return data;
	}

	struct mem_pool *pool = mem_pool_create(cache, flags);

	if (!pool)
		return 0;

	void *data = mem_pool_alloc(cache, pool);

	list_add(&pool->ll, &cache->partial_pools);
	return data;
}

void *mem_cache_alloc(struct mem_cache *cache, unsigned long alloc_flags)
{
	const unsigned long flags = spin_lock_save(&cache->lock);
	void *data = __mem_cache_alloc(cache, alloc_flags);

	spin_unlock_restore(&cache->lock, flags);
	return data;
}

static void __mem_cache_free(struct mem_cache *cache, void *ptr)
{
	const size_t pool_size = (size_t)1 << (cache->pool_order + PAGE_SHIFT);
	const uintptr_t addr = align_down((uintptr_t)ptr, pool_size);
	struct mem_pool *pool = (struct mem_pool *)(addr + cache->meta_offs);

	mem_pool_free(cache, pool, ptr);

	if (pool->free == 1) {
		list_del(&pool->ll);
		list_add(&pool->ll, &cache->partial_pools);
	}

	if (pool->free == cache->obj_count) {
		list_del(&pool->ll);
		list_add(&pool->ll, &cache->free_pools);
	}
}

void mem_cache_free(struct mem_cache *cache, void *ptr)
{
	const unsigned long flags = spin_lock_save(&cache->lock);

	__mem_cache_free(cache, ptr);
	spin_unlock_restore(&cache->lock, flags);
}


#define MEM_POOLS	(sizeof(mem_pool_size) / sizeof(mem_pool_size[0]))

static size_t mem_pool_size[] = {
	64,   128,  192,  256,  320,  384,  448,
	512,  576,  640,  704,  768,  832,  896,
	960,  1024, 1152, 1280, 1408, 1536, 1664,
	1792, 1920, 2048, 2304, 2560, 2816, 3072,
	3584, 4096, 5120, 6144, 7168, 8192
};
static struct mem_cache mem_pool[MEM_POOLS];


static struct mem_cache *mem_pool_lookup(size_t size)
{
	for (int i = 0; i != MEM_POOLS; ++i)
		if (mem_pool_size[i] >= size)
			return &mem_pool[i];
	return 0;
}

static int mem_order_calculate(size_t size)
{
	int order;

	for (order = 0; order != MAX_ORDER + 1; ++order)
		if (((size_t)1 << (order + PAGE_SHIFT)) >= size)
			break;
	return order;
}

void mem_alloc_setup(void)
{
	for (int i = 0; i != MEM_POOLS; ++i) {
		const size_t size = mem_pool_size[i];
		const size_t align = mem_pool_size[0];

		mem_cache_setup(&mem_pool[i], size, align);
	}
}

void *mem_alloc(size_t size)
{
	struct mem_cache *cache = mem_pool_lookup(size);

	if (cache)
		return mem_cache_alloc(cache, PA_ANY);

	const int order = mem_order_calculate(size);
	struct page *page = __page_alloc(order, PA_ANY);

	if (!page)
		return 0;

	page_clear_bit(page, PAGE_CACHE_BIT);
	page->u.order = order;

	return (void *)page_addr(page);
}

void mem_free(void *ptr)
{
	if (!ptr)
		return;

	struct page *page = addr_page((uintptr_t)ptr & ~(uintptr_t)PAGE_MASK);

	if (page_test_bit(page, PAGE_CACHE_BIT)) {
		mem_cache_free(page->u.cache, ptr);
		return;
	}

	page_clear_bit(page, PAGE_CACHE_BIT);
	page->u.order = 0;
	__page_free(page, page->u.order);
}

void *mem_realloc(void *ptr, size_t size)
{
	if (!ptr)
		return mem_alloc(size);

	struct page *page = addr_page((uintptr_t)ptr & ~(uintptr_t)PAGE_MASK);
	size_t old_size;

	if (page_test_bit(page, PAGE_CACHE_BIT)) {
		struct mem_cache *cache = page->u.cache;

		old_size = cache->obj_size;
		if (old_size >= size)
			return ptr;
	} else {
		const int order = page->u.order;

		old_size = (size_t)1 << (PAGE_SHIFT + order);
		if (old_size >= size)
			return ptr;
	}

	void *new = mem_alloc(size);

	if (!new)
		return 0;

	memcpy(new, ptr, old_size);
	mem_free(ptr);
	return new;
}
