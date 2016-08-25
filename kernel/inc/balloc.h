#ifndef __BOOTSTRAP_ALLOCATOR_H__
#define __BOOTSTRAP_ALLOCATOR_H__

#include <stdint.h>
#include <stddef.h>
#include <rbtree.h>
#include <list.h>

struct mboot_info;

struct memory_node {
	union {
		struct rb_node rb;
		struct list_head ll;
	} link;
	unsigned long long begin;
	unsigned long long end;
};

extern struct rb_tree memory_map;

void balloc_setup(const struct mboot_info *info);
uintptr_t __balloc_alloc(size_t size, uintptr_t align,
			uintptr_t from, uintptr_t to);
uintptr_t balloc_alloc(size_t size, uintptr_t from, uintptr_t to);
void balloc_free(uintptr_t begin, uintptr_t end);

#endif /*__BOOTSTRAP_ALLOCATOR_H__*/
