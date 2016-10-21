#include <stdatomic.h>
#include <lflist.h>
#include <stdint.h>


#define DELETED		((uintptr_t)1)


static inline struct lf_list_head *lf_marked(const struct lf_list_head *node)
{
	return (struct lf_list_head *)((uintptr_t)node | DELETED);
}

static inline struct lf_list_head *lf_unmarked(const struct lf_list_head *node)
{
	return (struct lf_list_head *)((uintptr_t)node & ~DELETED);
}

static inline int lf_is_marked(const struct lf_list_head *node)
{
	return ((uintptr_t)node & DELETED) != 0;
}

void lf_list_init(struct lf_list_head *lst)
{
	atomic_store_explicit(&lst->next, 0, memory_order_relaxed);
}

static int lf_search(struct lf_list_head *lst, const struct lf_list_head *key,
			lf_node_cmp_t cmp,
			struct lf_list_head **p, struct lf_list_head **c,
			struct lf_list_head **n)
{
	struct lf_list_head *prev = 0, *curr = 0, *next = 0;
	struct lf_list_head *ptr;

again:
	prev = lst;
	curr = atomic_load_explicit(&prev->next, memory_order_consume);

	while (1) {
		if (!lf_unmarked(curr)) {
			*p = lf_unmarked(prev);
			*c = lf_unmarked(curr);
			*n = lf_unmarked(next);
			return 0;
		}

		next = atomic_load_explicit(&lf_unmarked(curr)->next,
					memory_order_consume);
		ptr = atomic_load_explicit(&lf_unmarked(prev)->next,
					memory_order_relaxed);
		if (ptr != lf_unmarked(curr))
			goto again;

		if (!lf_is_marked(next)) {
			const int res = cmp(lf_unmarked(curr), key);

			if (res >= 0) {
				*p = lf_unmarked(prev);
				*c = lf_unmarked(curr);
				*n = lf_unmarked(next);
				return res == 0;
			}
			prev = curr;
		} else {
			ptr = lf_unmarked(curr);

			if (!atomic_compare_exchange_strong_explicit(
						&prev->next,
						&ptr,
						lf_unmarked(next),
						memory_order_release,
						memory_order_relaxed))
				goto again;
		}
		curr = next;
	}
}

struct lf_list_head *lf_list_lookup(struct lf_list_head *lst,
			const struct lf_list_head *key, lf_node_cmp_t cmp)
{
	struct lf_list_head *prev, *curr, *next;

	if (lf_search(lst, key, cmp, &prev, &curr, &next))
		return lf_unmarked(curr);
	return 0;
}

int lf_list_insert(struct lf_list_head *lst,
			struct lf_list_head *new, lf_node_cmp_t cmp)
{
	struct lf_list_head *prev, *curr, *next, *ptr;

	while (1) {
		if (lf_search(lst, new, cmp, &prev, &curr, &next))
			return 0;

		ptr = lf_unmarked(curr);
		atomic_store_explicit(&new->next, ptr, memory_order_relaxed);
		if (atomic_compare_exchange_strong_explicit(&prev->next,
					&ptr, new,
					memory_order_release,
					memory_order_relaxed))
			return 1;
	}
}

int lf_list_remove(struct lf_list_head *lst, const struct lf_list_head *key,
			lf_node_cmp_t cmp)
{
	struct lf_list_head *prev, *curr, *next, *ptr;

	while (1) {
		if (!lf_search(lst, key, cmp, &prev, &curr, &next))
			return 0;

		ptr = lf_unmarked(next);
		if (!atomic_compare_exchange_strong_explicit(&curr->next,
					&ptr, lf_marked(ptr),
					memory_order_relaxed,
					memory_order_relaxed))
			continue;

		ptr = lf_unmarked(curr);
		if (!atomic_compare_exchange_strong_explicit(&prev->next,
					&ptr, lf_unmarked(next),
					memory_order_release,
					memory_order_relaxed))
			lf_search(lst, key, cmp, &prev, &curr, &next);
		return 1;
	}
}
