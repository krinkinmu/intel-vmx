#include <dcache.h>
#include <string.h>
#include <alloc.h>
#include <debug.h>


static struct mem_cache dentry_cache;
static struct hash_table dentry_table;


static uint64_t dentry_hash(const struct dentry *parent, const char *name)
{
	static const uint64_t mul = 655360001;

	uint64_t name_hash = 0;

	while (*name)
		name_hash = (name_hash + (unsigned)(*name++)) * mul;

	return name_hash + (uint64_t)parent;
}

static int dentry_equal(const struct hash_node *l, const void *r)
{
	const struct dentry *lentry = (const struct dentry *)l;
	const struct dentry *rentry = (const struct dentry *)r;

	if (lentry == rentry)
		return 1;

	if (lentry->hash != rentry->hash)
		return 0;

	return lentry->parent == rentry->parent &&
				!strcmp(lentry->name, rentry->name);
}

void dcache_add(struct dentry *dentry)
{
	struct hash_node *inserted = hash_insert(&dentry_table, dentry->hash,
				&dentry->node, &dentry_equal);

	BUG_ON(inserted != &dentry->node);
}

struct dentry *dcache_lookup(struct dentry *parent, const char *name)
{
	const uint64_t hash = dentry_hash(parent, name);
	const struct dentry entry = {
		.hash = hash,
		.parent = parent,
		.name = name
	};

	struct hash_node *found = hash_lookup(&dentry_table, hash,
				&entry, &dentry_equal);

	if (!found)
		return 0;
	return CONTAINER_OF(found, struct dentry, node);
}

void dcache_delete(struct dentry *dentry)
{
	struct hash_node *deleted = hash_remove(&dentry_table, dentry->hash,
				&dentry, &dentry_equal);

	BUG_ON(&dentry->node != deleted);
}

struct dentry *dcache_alloc(void)
{
	struct dentry *dentry = mem_cache_alloc(&dentry_cache, PA_ANY);

	if (!dentry)
		return 0;
	return dentry;
}

void dcache_free(struct dentry *dentry)
{
	mem_cache_free(&dentry_cache, dentry);
}

void dcache_setup(void)
{
	const size_t size = sizeof(struct dentry);

	mem_cache_setup(&dentry_cache, size, size);
	hash_setup(&dentry_table);
}
