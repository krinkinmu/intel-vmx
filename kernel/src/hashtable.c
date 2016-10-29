#include <hashtable.h>
#include <spinlock.h>
#include <memory.h>
#include <string.h>
#include <debug.h>
#include <alloc.h>
#include <rcu.h>

#define HASH_BUCKETS	(PAGE_SIZE / sizeof(struct hash_bucket))
#define HASH_LOAD	2

struct hash_bucket {
	struct rwlock lock;
	struct hash_node guard;
};

struct hash_seg {
	struct hash_bucket bucket[HASH_BUCKETS];
};

struct hash_table_impl {
	struct rcu_callback rcu;
	size_t buckets;
	struct hash_seg * _Atomic seg[1];
};


static uint8_t hash_rev(uint8_t key)
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

static uint16_t hash_rev16(uint16_t key)
{
	const uint8_t low = key & 0xfful;
	const uint8_t high = key >> 8;

	return ((uint16_t)hash_rev(low) << 8) | hash_rev(high);
}

static uint32_t hash_rev32(uint32_t key)
{
	const uint16_t low = key & 0xfffful;
	const uint16_t high = key >> 16;

	return ((uint32_t)hash_rev16(low) << 16) | hash_rev16(high);
}

static uint64_t hash_rev64(uint64_t key)
{
	const uint32_t low = key & 0xfffffffful;
	const uint32_t high = key >> 32;

	return ((uint64_t)hash_rev32(low) << 32) | hash_rev32(high);
}

static uint64_t hash_bucket_key(size_t bucket)
{
	return hash_rev64(bucket) & ~((uint64_t)1);
}

static uint64_t hash_key(uint64_t hash)
{
	return hash_rev64(hash) | 1;
}

static void hash_bucket_setup(struct hash_bucket *bucket)
{
	rwlock_init(&bucket->lock);
	list_init(&bucket->guard.ll);
	bucket->guard.key = 0;
}

static int hash_bucket_uninitialized(struct hash_bucket *bucket)
{
	return list_empty(&bucket->guard.ll);
}

static struct hash_bucket *__hash_bucket(struct hash_table_impl *impl,
			size_t bucket_no)
{
	const size_t seg = bucket_no / HASH_BUCKETS;
	const size_t off = bucket_no % HASH_BUCKETS;
	struct hash_seg *new, *old = atomic_load_explicit(&impl->seg[seg],
				memory_order_consume);

	if (old)
		return &old->bucket[off];

	new = mem_alloc(sizeof(*new));
	if (!new)
		return 0;

	for (size_t i = 0; i != HASH_BUCKETS; ++i)
		hash_bucket_setup(&new->bucket[i]);

	if (atomic_compare_exchange_strong_explicit(&impl->seg[seg], &old, new,
				memory_order_release, memory_order_consume))
		return &new->bucket[off];
	mem_free(new);
	return &old->bucket[off];
}

static void __hash_bucket_insert(struct hash_bucket *bucket,
			struct hash_node *new)
{
	const uint64_t minkey = bucket->guard.key;
	struct list_head *head = &bucket->guard.ll;
	struct list_head *ptr = head->next;

	BUG_ON(minkey >= new->key);

	while (1) {
		struct hash_node *node = CONTAINER_OF(ptr,
					struct hash_node, ll);

		if (minkey >= node->key || node->key > new->key) {
			list_insert_before(&new->ll, &node->ll);
			break;
		}
		ptr = ptr->next;
	}
}

static size_t hash_parent(size_t bucket)
{
	size_t mask = bucket;

	mask |= mask >> 1;
	mask |= mask >> 2;
	mask |= mask >> 4;
	mask |= mask >> 8;
	mask |= mask >> 16;
	mask |= mask >> 32;

	return bucket & (mask >> 1);
}

static struct hash_bucket *hash_bucket(struct hash_table *table, uint64_t hash)
{
	struct hash_table_impl *impl;
	struct hash_bucket *bucket;
	size_t bucket_no;

	rcu_read_lock();
	impl = atomic_load_explicit(&table->table, memory_order_consume);
	BUG_ON(!impl);
	bucket_no = hash & (impl->buckets - 1);
	bucket = __hash_bucket(impl, bucket_no);
	rcu_read_unlock();

	if (!bucket)
		return 0;

	if (!bucket_no || !hash_bucket_uninitialized(bucket))
		return bucket;

	const size_t parent_no = hash_parent(bucket_no);
	struct hash_bucket *parent = hash_bucket(table, parent_no);

	if (!parent)
		return 0;

	const unsigned long flags = write_lock_save(&parent->lock);

	write_lock(&bucket->lock);
	if (hash_bucket_uninitialized(bucket)) {
		bucket->guard.key = hash_bucket_key(bucket_no);
		__hash_bucket_insert(parent, &bucket->guard);
	}
	write_unlock(&bucket->lock);
	write_unlock_restore(&parent->lock, flags);
	return bucket;
}

static void __hash_release_impl(struct rcu_callback *rcu)
{
	struct hash_table_impl *impl = (struct hash_table_impl *)rcu;

	mem_free(impl);
}

static void __hash_grow(struct hash_table *table)
{
	struct hash_table_impl *old = atomic_load_explicit(&table->table,
				memory_order_consume);
	const size_t entries = atomic_load_explicit(&table->entries,
				memory_order_relaxed);

	const size_t oldbuckets = old ? old->buckets : 0;
	const size_t newbuckets = old ? oldbuckets * 2 : 4096;

	if (oldbuckets * HASH_LOAD > entries)
		return;

	const size_t newsize = sizeof(*old) +
				(newbuckets - 1) * sizeof(struct hash_bucket);
	const size_t oldsize = !old ? 0 : sizeof(*old) +
				(oldbuckets - 1) * sizeof(struct hash_bucket);

	struct hash_table_impl *new = mem_alloc(newsize);

	if (!new)
		return;

	BUG_ON(!new);
	memset(new, 0, newsize);
	memcpy(new, old, oldsize);
	new->buckets = newbuckets;
	atomic_store_explicit(&table->table, new, memory_order_release);

	if (old)
		rcu_call(&old->rcu, &__hash_release_impl);
}

static void hash_grow(struct hash_table *table)
{
	struct hash_table_impl *impl;
	size_t entries;
	int resize = 0;

	rcu_read_lock();
	impl = atomic_load_explicit(&table->table, memory_order_consume);
	entries = atomic_load_explicit(&table->entries, memory_order_relaxed);
	if (impl->buckets * HASH_LOAD < entries)
		resize = atomic_flag_test_and_set_explicit(&table->resizing,
					memory_order_acquire);
	rcu_read_unlock();

	if (!resize)
		return;
	__hash_grow(table);
	atomic_flag_clear_explicit(&table->resizing, memory_order_release);
}

void hash_setup(struct hash_table *table)
{
	atomic_store_explicit(&table->table, 0, memory_order_relaxed);
	atomic_store_explicit(&table->entries, 0, memory_order_relaxed);
	atomic_flag_clear_explicit(&table->resizing, memory_order_relaxed);
	__hash_grow(table);
	BUG_ON(!atomic_load_explicit(&table->table, memory_order_relaxed));
}

static void __hash_release(struct rcu_callback *rcu)
{
	struct hash_table_impl *impl = (struct hash_table_impl *)rcu;
	const size_t buckets = impl->buckets;
	const size_t segments = (buckets + HASH_BUCKETS - 1) / HASH_BUCKETS;

	for (size_t i = 0; i != segments; ++i)
		mem_free(impl->seg[i]);
	mem_free(impl);
}

void hash_release(struct hash_table *table)
{
	struct hash_table_impl *impl = atomic_load_explicit(&table->table,
				memory_order_relaxed);

	if (impl)
		rcu_call(&impl->rcu, &__hash_release);
}

struct hash_node *hash_insert(struct hash_table *table, uint64_t hash,
			struct hash_node *new, found_fptr_t equal)
{
	struct hash_bucket *bucket = hash_bucket(table, hash);

	if (!bucket)
		return 0;

	new->key = hash_key(hash);

	const unsigned long flags = write_lock_save(&bucket->lock);
	const uint64_t minkey = bucket->guard.key;
	struct list_head *head = &bucket->guard.ll;
	struct list_head *ptr = head->next;

	BUG_ON(minkey >= new->key);

	while (1) {
		struct hash_node *node = CONTAINER_OF(ptr,
					struct hash_node, ll);

		if (node->key <= minkey || node->key > new->key) {
			list_insert_before(&new->ll, &node->ll);
			atomic_fetch_add_explicit(&table->entries, 1,
						memory_order_relaxed);
			break;
		}

		if (node->key == new->key && equal && equal(node, new)) {
			new = node;
			break;
		}
		ptr = ptr->next;
	}

	write_unlock_restore(&bucket->lock, flags);
	hash_grow(table);
	return new;
}

struct hash_node *hash_remove(struct hash_table *table, uint64_t hash,
			const void *key, found_fptr_t equal)
{
	struct hash_bucket *bucket = hash_bucket(table, hash);

	if (!bucket)
		return 0;

	const uint64_t target = hash_key(hash);

	const unsigned long flags = write_lock_save(&bucket->lock);
	const uint64_t minkey = bucket->guard.key;
	struct list_head *head = &bucket->guard.ll;
	struct list_head *ptr = head->next;
	struct hash_node *res = 0;

	while (ptr != head) {
		struct hash_node *node = CONTAINER_OF(ptr,
					struct hash_node, ll);

		if (node->key <= minkey || node->key > target)
			break;

		if (node->key == target && equal(node, key)) {
			atomic_fetch_sub_explicit(&table->entries, 1,
						memory_order_relaxed);
			list_del(ptr);
			res = node;
			break;
		}

		ptr = ptr->next;
	}
	write_unlock_restore(&bucket->lock, flags);
	return res;
}

struct hash_node *hash_lookup(struct hash_table *table, uint64_t hash,
			const void *key, found_fptr_t equal)
{
	struct hash_bucket *bucket = hash_bucket(table, hash);

	if (!bucket)
		return 0;

	const uint64_t target = hash_key(hash);

	const unsigned long flags = read_lock_save(&bucket->lock);
	const uint64_t minkey = bucket->guard.key;
	struct list_head *head = &bucket->guard.ll;
	struct list_head *ptr = head->next;
	struct hash_node *res = 0;

	while (ptr != head) {
		struct hash_node *node = CONTAINER_OF(ptr,
					struct hash_node, ll);

		if (node->key <= minkey || node->key > target)
			break;

		if (node->key == target && equal(node, key)) {
			res = node;
			break;
		}

		ptr = ptr->next;
	}
	read_unlock_restore(&bucket->lock, flags);
	return res;
}
