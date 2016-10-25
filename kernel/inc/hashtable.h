#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

#include <stdatomic.h>
#include <stdint.h>
#include <list.h>

#define HASH_BUCKETS	(PAGE_SIZE / sizeof(struct hash_bucket))

struct hash_node {
	struct list_head ll;
	uint64_t key;
};

struct hash_table_impl;
struct hash_table {
	struct hash_table_impl * _Atomic table;
	atomic_size_t entries;
	atomic_flag resizing;
};

void hash_setup(struct hash_table *table);
void hash_release(struct hash_table *table);

typedef int (*found_fptr_t)(const struct hash_node *node, const void *key);

struct hash_node *hash_insert(struct hash_table *table, uint64_t hash,
			struct hash_node *node, found_fptr_t equal);
struct hash_node *hash_remove(struct hash_table *table, uint64_t hash,
			const void *key, found_fptr_t equal);
struct hash_node *hash_lookup(struct hash_table *table, uint64_t hash,
			const void *key, found_fptr_t equal);

#endif /*__HASHTABLE_H__*/
