#include <stdlib.h>
#include <string.h>
#include <percpu.h>
#include <hazptr.h>
#include <alloc.h>
#include <debug.h>


#define HP_PTRS		32
#define HP_DELETED	(MAX_CPU_NR * HP_PTRS)

struct hp_deleted {
	hp_free_fptr_t free;
	void *ptr;
	void *arg;
};

struct hp_protected {
	void *ptr;
};

struct hp_plist {
	struct hp_protected *protected;
	int size, capacity, order;
};

static void hp_setup_rlist(struct hp_rlist *rlist)
{
	rlist->deleted = 0;
	rlist->size = rlist->capacity = 0;
	rlist->order = 0;
}

static int hp_rlist_add(struct hp_rlist *rlist, struct hp_deleted *deleted)
{
	if (rlist->size == rlist->capacity) {
		const int old_order = rlist->order;
		const int new_order = old_order + 1;
		const size_t new_size = (size_t)1 << (PAGE_SHIFT + new_order);

		struct hp_deleted *deleted =
			(struct hp_deleted *)page_alloc(new_order, PA_ANY);

		if (!deleted)
			return -1;

		if (rlist->deleted) {
			memcpy(deleted, rlist->deleted,
					rlist->size * sizeof(*deleted));
			page_free((uintptr_t)rlist->deleted, old_order);
		}
		rlist->capacity = new_size / sizeof(*deleted);
		rlist->deleted = deleted;
		rlist->order = new_order;
	}

	rlist->deleted[rlist->size++] = *deleted;
	return 0;
}

static int hp_deleted_cmp(const void *lptr, const void *rptr)
{
	const struct hp_deleted *l = lptr;
	const struct hp_deleted *r = rptr;

	if (l->ptr != r->ptr)
		return l->ptr < r->ptr ? -1 : 1;
	return 0;
}

static void hp_rlist_sort(struct hp_rlist *rlist)
{
	qsort(rlist->deleted, rlist->size, sizeof(*rlist->deleted),
				&hp_deleted_cmp);
}

static void hp_setup_plist(struct hp_plist *plist)
{
	plist->protected = 0;
	plist->size = plist->capacity = 0;
	plist->order = 0;
}

static int hp_plist_add(struct hp_plist *plist, struct hp_protected *protected)
{
	if (plist->size == plist->capacity) {
		const int old_order = plist->order;
		const int new_order = old_order + 1;
		const size_t new_size = (size_t)1 << (PAGE_SHIFT + new_order);

		struct hp_protected *protected =
			(struct hp_protected *)page_alloc(new_order, PA_ANY);

		if (!protected)
			return -1;

		if (plist->protected) {
			memcpy(protected, plist->protected,
					plist->size * sizeof(*protected));
			page_free((uintptr_t)plist->protected, old_order);
		}
		plist->capacity = new_size / sizeof(*protected);
		plist->protected = protected;
		plist->order = new_order;
	}

	plist->protected[plist->size++] = *protected;
	return 0;
}

static int hp_protected_cmp(const void *lptr, const void *rptr)
{
	const struct hp_protected *l = lptr;
	const struct hp_protected *r = rptr;

	if (l->ptr != r->ptr)
		return l->ptr < r->ptr ? -1 : 1;
	return 0;
}

static void hp_plist_sort(struct hp_plist *plist)
{
	qsort(plist->protected, plist->size, sizeof(*plist->protected),
				&hp_protected_cmp);
}

static void hp_setup_cpu_context(struct hp_cpu_context *ctx)
{
	hp_setup_rlist(&ctx->rlist);
	list_init(&ctx->full);
	list_init(&ctx->ready);
}

void hp_setup_domain(struct hp_domain *domain)
{
	atomic_store_explicit(&domain->blocks, 0, memory_order_release);
	for (int i = 0; i != MAX_CPU_NR; ++i)
		hp_setup_cpu_context(&domain->cpu_ctx[i]);
}


static __percpu struct mem_cache hp_block_cache;
static __percpu struct hp_plist cpu_plist;
static struct hp_domain default_domain;


static void hp_setup_block(struct hp_block *block, struct hp_cpu_context *ctx)
{
	block->next = 0;
	block->free = HP_PTRS;
	block->cpu_ctx = ctx;

	for (int i = 0; i != HP_PTRS; ++i) {
		struct hp_rec *rec = &block->hp_rec[i];

		atomic_store(&rec->ptr, 0);
		rec->owner = block;
		rec->free = 1;
	}
}

static void hp_attach_block(struct hp_block *block, struct hp_domain *domain)
{
	struct hp_block *next = atomic_load_explicit(&domain->blocks,
			memory_order_relaxed);

	do {
		block->next = next;
	} while (!atomic_compare_exchange_weak_explicit(&domain->blocks,
			&next, block, memory_order_release,
			memory_order_relaxed));
}

static struct hp_block *hp_alloc_block(struct hp_cpu_context *ctx)
{
	struct hp_block *block = mem_cache_alloc(&hp_block_cache, PA_ANY);

	if (!block)
		return 0;

	hp_setup_block(block, ctx);
	return block;
}

static struct hp_rec *hp_alloc_rec(struct hp_block *block)
{
	for (int i = 0; i != HP_PTRS; ++i) {
		struct hp_rec *rec = &block->hp_rec[i];

		if (rec->free) {
			rec->free = 0;
			--block->free;
			return rec;
		}
	}
	return 0;
}

static struct hp_rec *___hp_acquire(struct hp_domain *domain)
{
	struct hp_cpu_context *ctx = &domain->cpu_ctx[cpu_id()];

	if (list_empty(&ctx->ready)) {
		struct hp_block *block = hp_alloc_block(ctx);

		if (!block)
			return 0;

		list_add(&block->ll, &ctx->ready);
		hp_attach_block(block, domain);
		return hp_alloc_rec(block);
	}

	struct hp_block *block = LIST_ENTRY(ctx->ready.next,
				struct hp_block, ll);
	struct hp_rec *rec = hp_alloc_rec(block);

	if (!block->free) {
		list_del(&block->ll);
		list_add(&block->ll, &ctx->full);
	}
	return rec;
}

struct hp_rec *__hp_acquire(struct hp_domain *domain)
{
	const unsigned long flags = local_int_save();
	struct hp_rec *hp_rec = ___hp_acquire(domain);

	local_int_restore(flags);
	return hp_rec;
}

struct hp_rec *hp_acquire(void)
{
	return __hp_acquire(&default_domain);
}

static void __hp_release(struct hp_rec *rec)
{
	struct hp_block *block = rec->owner;
	struct hp_cpu_context *ctx = block->cpu_ctx;

	BUG_ON(rec->free);
	hp_clear(rec);
	rec->free = 1;

	if (++block->free == 1) {
		list_del(&block->ll);
		list_add_tail(&block->ll, &ctx->ready);
	}
}

void hp_release(struct hp_rec *rec)
{
	const unsigned long flags = local_int_save();

	__hp_release(rec);
	local_int_restore(flags);
}

void *hp_get(struct hp_rec *hp_rec)
{
	return atomic_load(&hp_rec->ptr);
}

void hp_set(struct hp_rec *hp_rec, void *ptr)
{
	BUG_ON(hp_rec->free);
	atomic_store(&hp_rec->ptr, ptr);
}

void hp_clear(struct hp_rec *hp_rec)
{
	hp_set(hp_rec, 0);
}

static void ___hp_retire(struct hp_domain *domain, struct hp_deleted *deleted)
{
	struct hp_cpu_context *ctx = &domain->cpu_ctx[cpu_id()];
	struct hp_rlist *rlist = &ctx->rlist;

	if (hp_rlist_add(rlist, deleted)) {
		BUG_ON(rlist->size != rlist->capacity);

		while (rlist->size == rlist->capacity)
			__hp_gc(domain);
		BUG_ON(hp_rlist_add(rlist, deleted));
	}

	if (rlist->size >= HP_DELETED)
		__hp_gc(domain);
}

void __hp_retire(struct hp_domain *domain, void *ptr, hp_free_fptr_t free,
			void *arg)
{
	const unsigned long flags = local_int_save();
	struct hp_deleted deleted = {
		.free = free,
		.ptr = ptr,
		.arg = arg
	};

	___hp_retire(domain, &deleted);
	local_int_restore(flags);
}

void hp_retire(void *ptr, hp_free_fptr_t free, void *arg)
{
	__hp_retire(&default_domain, ptr, free, arg);
}

static void ___hp_gc(struct hp_domain *domain)
{
	struct hp_cpu_context *ctx = &domain->cpu_ctx[cpu_id()];
	struct hp_rlist *rlist = &ctx->rlist;
	struct hp_plist *plist = &cpu_plist;
	struct hp_block *block = atomic_load_explicit(&domain->blocks,
				memory_order_consume);

	plist->size = 0;
	while (block) {
		for (int i = 0; i != HP_PTRS; ++i) {
			void *ptr = hp_get(&block->hp_rec[i]);

			if (!ptr)
				continue;

			struct hp_protected prot = {.ptr = ptr};

			if (hp_plist_add(plist, &prot))
				return;
		}
		block = block->next;
	}

	hp_rlist_sort(rlist);
	hp_plist_sort(plist);

	int released = 0;

	for (int r = 0, p = 0; r != rlist->size; ++r) {
		struct hp_deleted *del= &rlist->deleted[r];

		while (p != plist->size && plist->protected[p].ptr < del->ptr)
			++p;

		if (p != plist->size && plist->protected[p].ptr == del->ptr) {
			if (released)
				rlist->deleted[r - released] = *del;
		} else {
			del->free(del->ptr, del->arg);
			++released;
		}
	}
	rlist->size -= released;
}

void __hp_gc(struct hp_domain *domain)
{
	const unsigned long flags = local_int_save();

	___hp_gc(domain);
	local_int_restore(flags);
}

void hp_gc(void)
{
	__hp_gc(&default_domain);
}

void hp_cpu_setup(void)
{
	const size_t size = sizeof(struct hp_block);
	const size_t align = 64;

	mem_cache_setup(&hp_block_cache, size, align);
	hp_setup_plist(&cpu_plist);
}

void hp_setup(void)
{
	hp_setup_domain(&default_domain);
}
