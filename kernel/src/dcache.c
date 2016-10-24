#include <spinlock.h>
#include <memory.h>
#include <dcache.h>
#include <string.h>
#include <alloc.h>
#include <debug.h>
#include <rcu.h>


#define BUCKETS        (PAGE_SIZE / sizeof(struct dcache_bucket))
#define LOAD_FACTOR    2


struct dcache_bucket {
	struct rwlock lock;
	struct dcache_node node;
};

struct dcache_seg {
	struct dcache_bucket bucket[BUCKETS];
};

struct dcache_table {
	struct rcu_callback rcu;
	size_t buckets;
	struct dcache_seg * _Atomic seg[1];
};


static struct mem_cache dentry_cache;
static struct dcache_table * _Atomic table;
static struct spinlock resize_lock;
static atomic_size_t entries;


static size_t dcache_table_size(size_t segs)
{
	if (!segs)
		return 0;

	return sizeof(struct dcache_table) +
				(segs - 1) * sizeof(struct dcache_seg *);
}

static size_t dcache_table_segs(size_t buckets)
{
	return (buckets + BUCKETS - 1) / BUCKETS;
}

static void dcache_rcu_callback(struct rcu_callback *rcu)
{
	struct dcache_table *table = CONTAINER_OF(rcu,
				struct dcache_table, rcu);

	mem_free(table);
}

static void __dcache_grow(void)
{
	static const size_t initial_size = 4096;

	struct dcache_table *old = atomic_load_explicit(&table,
				memory_order_consume);

	const size_t oldbuckets = old ? old->buckets : 0;
	const size_t newbuckets = oldbuckets ? oldbuckets * 2 : initial_size;

	const size_t oldsegs = dcache_table_segs(oldbuckets);
	const size_t newsegs = dcache_table_segs(newbuckets);

	const size_t oldsize = dcache_table_size(oldsegs);
	const size_t newsize = dcache_table_size(newsegs);

	struct dcache_table *new = mem_alloc(newsize);

	if (!new)
		return;

	memset(new, 0, newsize);
	memcpy(new, old, oldsize);
	new->buckets = newbuckets;
	atomic_store_explicit(&table, new, memory_order_release);

	if (old)
		rcu_call(&old->rcu, &dcache_rcu_callback);
}

static void dcache_grow(void)
{
	struct dcache_table *old;
	size_t entries;

	rcu_read_lock();
	old = atomic_load_explicit(&table, memory_order_consume);
	entries = atomic_load_explicit(&entries, memory_order_relaxed);

	if (old->buckets * LOAD_FACTOR < entries) {
		const unsigned long flags = spin_lock_save(&resize_lock);

		old = atomic_load_explicit(&table, memory_order_relaxed);
		entries = atomic_load_explicit(&entries, memory_order_relaxed);
		if (old->buckets * LOAD_FACTOR < entries)
			__dcache_grow();
		spin_unlock_restore(&resize_lock, flags);
	}
	rcu_read_unlock();
}

static void __dcache_insert(struct dcache_bucket *bucket,
			struct dcache_node *new)
{
	const uint64_t minkey = bucket->node.key;
	struct list_head *head = &bucket->node.ll;
	struct list_head *ptr = head->next;

	while (ptr != head) {
		struct dcache_node *node = CONTAINER_OF(ptr,
					struct dcache_node, ll);

		if (node->key < minkey)
			break;
		ptr = ptr->next;
		if (node->key < new->key)
			continue;

		list_insert_before(&new->ll, &node->ll);
		return;
	}
	list_insert_before(&new->ll, ptr);
}

static uint8_t __dcache_rev(uint8_t key)
{
	static const uint8_t rvtable[] = {
		0, 128, 64, 192, 32, 160, 96, 224,
		16, 144, 80, 208, 48, 176, 112, 240,
		8, 136, 72, 200, 40, 168, 104, 232,
		24, 152, 88, 216, 56, 184, 120, 248,
		4, 132, 68, 196, 36, 164, 100, 228,
		20, 148, 84, 212, 52, 180, 116, 244,
		12, 140, 76, 204, 44, 172, 108, 236,
		28, 156, 92, 220, 60, 188, 124, 252,
		2, 130, 66, 194, 34, 162, 98, 226,
		18, 146, 82, 210, 50, 178, 114, 242,
		10, 138, 74, 202, 42, 170, 106, 234,
		26, 154, 90, 218, 58, 186, 122, 250,
		6, 134, 70, 198, 38, 166, 102, 230,
		22, 150, 86, 214, 54, 182, 118, 246,
		14, 142, 78, 206, 46, 174, 110, 238,
		30, 158, 94, 222, 62, 190, 126, 254,
		1, 129, 65, 193, 33, 161, 97, 225,
		17, 145, 81, 209, 49, 177, 113, 241,
		9, 137, 73, 201, 41, 169, 105, 233,
		25, 153, 89, 217, 57, 185, 121, 249,
		5, 133, 69, 197, 37, 165, 101, 229,
		21, 149, 85, 213, 53, 181, 117, 245,
		13, 141, 77, 205, 45, 173, 109, 237,
		29, 157, 93, 221, 61, 189, 125, 253,
		3, 131, 67, 195, 35, 163, 99, 227,
		19, 147, 83, 211, 51, 179, 115, 243,
		11, 139, 75, 203, 43, 171, 107, 235,
		27, 155, 91, 219, 59, 187, 123, 251,
		7, 135, 71, 199, 39, 167, 103, 231,
		23, 151, 87, 215, 55, 183, 119, 247,
		15, 143, 79, 207, 47, 175, 111, 239,
		31, 159, 95, 223, 63, 191, 127, 255
	};

	return rvtable[key];
}

static uint16_t __dcache_rev16(uint16_t key)
{
	const uint8_t low = key & 0xff;
	const uint8_t high = key >> 8;

	return ((uint16_t)__dcache_rev(low) << 8) | __dcache_rev(high);
}

static uint32_t __dcache_rev32(uint32_t key)
{
	const uint16_t low = key & 0xffffu;
	const uint16_t high = key >> 16;

	return ((uint32_t)__dcache_rev16(low) << 16) | __dcache_rev16(high);
}

static uint64_t __dcache_rev64(uint64_t key)
{
	const uint32_t low = key & 0xfffffffful;
	const uint32_t high = key >> 32;

	return ((uint64_t)__dcache_rev32(low) << 32) | __dcache_rev32(high);
}

static uint64_t dcache_dummy_key(uint64_t bucket)
{
	return __dcache_rev64(bucket) & ~((uint64_t)1);
}

static uint64_t dcache_key(uint64_t hash)
{
	return __dcache_rev64(hash) | 1;
}

static uint64_t dcache_hash(const struct dentry *parent, const char *name)
{
	static const uint64_t mul = 655360001;
	uint64_t name_hash = 0;

	while (*name)
		name_hash = (name_hash + (unsigned)(*name++)) * mul;

	return name_hash + (uint64_t)parent;
}

static struct dcache_seg *dcache_seg(struct dcache_table *table, size_t seg)
{
	struct dcache_seg *old = atomic_load_explicit(&table->seg[seg],
				memory_order_consume);
	struct dcache_seg *new;

	if (!old)
		return old;

	new = mem_alloc(sizeof(*new));
	if (!new)
		return new;

	for (int i = 0; i != BUCKETS; ++i) {
		const size_t bucket = seg * BUCKETS + i;

		rwlock_init(&new->bucket[i].lock);
		list_init(&new->bucket[i].node.ll);
		new->bucket[i].node.key = dcache_dummy_key(bucket);
	}

	if (atomic_compare_exchange_strong_explicit(&table->seg[seg], &old, new,
				memory_order_release, memory_order_consume))
		return new;

	mem_free(new);
	return old;
}

static struct dcache_bucket *__dcache_bucket(struct dcache_table *table,
			size_t no)
{
	const size_t segno = no / BUCKETS;
	const size_t bucno = no % BUCKETS;

	struct dcache_seg *seg = dcache_seg(table, segno);

	if (!seg)
		return 0;
	return &seg->bucket[bucno]; 
}

static int dcache_bucket_initialized(const struct dcache_bucket *bucket)
{
	return !list_empty(&bucket->node.ll);
}

static int dcache_bucket_init(size_t no, struct dcache_bucket *bucket)
{
	if (!no)
		return 0;

	const size_t pno = (no - 1) / 2;
	struct dcache_table *tbl;
	struct dcache_bucket *parent;

	rcu_read_lock();
	tbl = atomic_load_explicit(&table, memory_order_consume);
	parent = __dcache_bucket(tbl, pno);
	rcu_read_unlock();

	if (!parent)
		return -1;

	if (!dcache_bucket_initialized(parent)) {
		if (dcache_bucket_init(pno, parent))
			return -1;
	}

	const unsigned long flags = write_lock_save(&parent->lock);

	write_lock(&bucket->lock);
	if (!dcache_bucket_initialized(bucket))
		__dcache_insert(parent, &bucket->node);
	write_unlock(&bucket->lock);
	write_unlock_restore(&parent->lock, flags);
	return 0;
}

static struct dcache_bucket *dcache_bucket(uint64_t hash)
{
	struct dcache_table *tbl;
	struct dcache_bucket *bucket;
	size_t no;

	rcu_read_lock();
	tbl = atomic_load_explicit(&table, memory_order_consume);
	no = hash & (tbl->buckets - 1);
	bucket = __dcache_bucket(tbl, no);
	rcu_read_unlock();

	if (!bucket)
		return 0;

	if (!dcache_bucket_initialized(bucket)) {
		if (dcache_bucket_init(no, bucket))
			return 0;
	}
	return bucket;
}

void dcache_add(struct dentry *dentry)
{
	struct dcache_bucket *bucket = dcache_bucket(dentry->hash);

	BUG_ON(!bucket);
	dentry->node.key = dcache_key(dentry->hash);

	const unsigned long flags = write_lock_save(&bucket->lock);

	__dcache_insert(bucket, &dentry->node);
	atomic_fetch_add_explicit(&entries, 1, memory_order_relaxed);
	write_unlock_restore(&bucket->lock, flags);
	dcache_grow();
}

static struct dentry *__dcache_lookup(struct dcache_bucket *bucket,
			uint64_t key, struct dentry *parent, const char *name)
{
	const uint64_t minkey = bucket->node.key;
	struct list_head *head = &bucket->node.ll;
	struct list_head *ptr = head->next;

	while (ptr != head) {
		struct dcache_node *node = CONTAINER_OF(ptr,
					struct dcache_node, ll);

		if (node->key < minkey)
			break;
		ptr = ptr->next;
		if (node->key < key)
			continue;
		if (node->key > key)
			break;

		struct dentry *entry = CONTAINER_OF(node, struct dentry, node);

		if (entry->parent == parent && !strcmp(entry->name, name))
			return entry;
	}

	return 0;
}

struct dentry *dcache_lookup(struct dentry *parent, const char *name)
{
	const uint64_t hash = dcache_hash(parent, name);
	const uint64_t key = dcache_key(hash);

	struct dcache_bucket *bucket = dcache_bucket(hash);

	const unsigned long flags = read_lock_save(&bucket->lock);
	struct dentry *dentry = __dcache_lookup(bucket, key, parent, name);

	read_unlock_restore(&bucket->lock, flags);
	return dentry;
}

static void __dcache_remove(struct dcache_bucket *bucket,
			struct dcache_node *node)
{
	(void) bucket;
	list_del(&node->ll);
}

void dcache_delete(struct dentry *dentry)
{
	const uint64_t hash = dentry->hash;

	struct dcache_bucket *bucket = dcache_bucket(hash);

	const unsigned long flags = write_lock_save(&bucket->lock);

	__dcache_remove(bucket, &dentry->node);
	atomic_fetch_sub_explicit(&entries, 1, memory_order_relaxed);
	write_unlock_restore(&bucket->lock, flags);
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
	__dcache_grow();
}
