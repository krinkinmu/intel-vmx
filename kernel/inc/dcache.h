#ifndef __DENTRY_CACHE_H__
#define __DENTRY_CACHE_H__

#include <hashtable.h>

#define DCACHE_INLINE_LEN	88

struct dentry {
	struct hash_node node;
	struct dentry *parent;
	uint64_t hash;
	const char *name;
	char name_buf[DCACHE_INLINE_LEN];
};

struct dentry *dcache_alloc(void);
void dcache_free(struct dentry *dentry);

struct dentry *dcache_lookup(struct dentry *parent, const char *name);
void dcache_add(struct dentry *dentry);
void dcache_delete(struct dentry *dentry);

void dcache_setup(void);

#endif /*__DENTRY_CACHE_H__*/
