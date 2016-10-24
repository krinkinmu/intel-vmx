#ifndef __DENTRY_CACHE_H__
#define __DENTRY_CACHE_H__

#include <stddef.h>
#include <list.h>

#define DCACHE_INLINE_LEN	88

struct dcache_node {
	struct list_head ll;
	uint64_t key;
};

struct dentry {
	struct dcache_node node;
	struct dentry *parent;
	uint64_t hash;
	char *name;
	char name_buf[DCACHE_INLINE_LEN];
};

struct dentry *dcache_alloc(void);
void dcache_free(struct dentry *dentry);

struct dentry *dcache_lookup(struct dentry *parent, const char *name);
void dcache_add(struct dentry *dentry);
void dcache_delete(struct dentry *dentry);

void dcache_setup(void);

#endif /*__DENTRY_CACHE_H__*/
