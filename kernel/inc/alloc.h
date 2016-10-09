#ifndef __ALLOC_H__
#define __ALLOC_H__

#include <stddef.h>
#include <spinlock.h>
#include <memory.h>
#include <list.h>

struct mem_cache {
	struct spinlock lock;
	struct list_head free_pools;
	struct list_head partial_pools;
	struct list_head busy_pools;

	/* struct alloc_pool layout */
	size_t meta_offs;
	size_t obj_count;
	size_t mask_words;
	size_t obj_size;
	int pool_order;
};

void mem_cache_setup(struct mem_cache *cache, size_t size, size_t align);
void mem_cache_release(struct mem_cache *cache);

void *mem_cache_alloc(struct mem_cache *cache, unsigned long flags);
void mem_cache_free(struct mem_cache *cache, void *ptr);

void mem_alloc_setup(void);
void *mem_alloc(size_t size);
void *mem_realloc(void *ptr, size_t size);
void mem_free(void *ptr);

#endif /*__ALLOC_H__*/
