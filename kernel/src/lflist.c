#include <stdatomic.h>
#include <lflist.h>
#include <stdint.h>


#define DELETED		((uintptr_t)1)


static inline struct lf_node *lf_marked(const struct lf_node *node)
{
	return (struct lf_node *)((uintptr_t)node & ~DELETED);
}

static inline struct lf_node *lf_unmarked(const struct lf_node *node)
{
	return (struct lf_node *)((uintptr_t)node | DELETED);
}

static inline int lf_is_marked(const struct lf_node *node)
{
	return ((uintptr_t)node & DELETED) != 0;
}

void lf_list_setup(struct lf_list *lst, lf_node_cmp_t cmp)
{
	atomic_store_explicit(&lst->head.next, &lst->tail,
				memory_order_relaxed);
	atomic_store_explicit(&lst->tail.next, 0,
				memory_order_relaxed);
	lst->cmp = cmp;
}

static void lf_search(struct lf_list *lst, const struct lf_node *key,
			struct lf_node **prev, struct lf_node **next)
{
	struct lf_node *prev_next, *next_next;

again:
	while (1) {
		struct lf_node *p = &lst->head;
		struct lf_node *n = atomic_load_explicit(&lst->head.next,
					memory_order_consume);

		do {
			if (!lf_is_marked(n)) {
				*prev = p;
				prev_next = n;
			}

			p = lf_unmarked(n);
			if (p == &lst->tail)
				break;

			n = atomic_load_explicit(&p->next,
						memory_order_consume);
		} while (lf_is_marked(n) || lst->cmp(p, key) < 0);

		*next = p;

		if (prev_next == p) {
			next_next = atomic_load_explicit(&p->next,
						memory_order_relaxed);
			if ((p != &lst->tail) && lf_is_marked(next_next))
				goto again;
			else
				return;
		}

		if (atomic_compare_exchange_strong_explicit(&(*prev)->next,
					&prev_next, p,
					memory_order_release,
					memory_order_relaxed)) {
			next_next = atomic_load_explicit(&p->next,
						memory_order_relaxed);
			if ((p != &lst->tail) && lf_is_marked(next_next))
				goto again;
			else
				return;
		}
	}
}

struct lf_node *lf_list_lookup(struct lf_list *lst, const struct lf_node *key)
{
	struct lf_node *prev, *next;

	lf_search(lst, key, &prev, &next);
	if ((next == &lst->tail) || lst->cmp(next, key))
		return 0;
	return next;
}

struct lf_node *lf_list_insert(struct lf_list *lst, struct lf_node *key)
{
	struct lf_node *prev, *next;

	while (1) {
		lf_search(lst, key, &prev, &next);

		if ((next != &lst->tail) && !lst->cmp(next, key))
			return next;

		atomic_store_explicit(&key->next, next, memory_order_release);
		if (atomic_compare_exchange_strong_explicit(&prev->next,
					&next, key,
					memory_order_release,
					memory_order_relaxed))
			return key;
	}
}

struct lf_node *lf_list_remove(struct lf_list *lst, const struct lf_node *key)
{
	struct lf_node *prev, *next, *next_next;

	while (1) {
		lf_search(lst, key, &prev, &next);
		if ((next == &lst->tail) || lst->cmp(key, next))
			return 0;

		next_next = atomic_load_explicit(&next->next,
					memory_order_relaxed);
		if (!lf_is_marked(next_next))
			if (atomic_compare_exchange_strong_explicit(&next->next,
						&next_next,
						lf_marked(next_next),
						memory_order_release,
						memory_order_relaxed))
				break;
	}

	if (!atomic_compare_exchange_strong_explicit(&prev->next,
				&next,
				next_next,
				memory_order_release,
				memory_order_relaxed))
		lf_search(lst, next, &prev, &next);
	return next;
}
