#ifndef __LOCK_FREE_LIST_H__
#define __LOCK_FREE_LIST_H__


struct lf_node {
	struct lf_node * _Atomic next;
};

struct lf_list {
	struct lf_node head;
	struct lf_node tail;
};


typedef int (*lf_node_cmp_t)(const struct lf_node *, const struct lf_node *);

void lf_list_init(struct lf_list *lst);
struct lf_node *lf_list_lookup(struct lf_list *lst, const struct lf_node *key,
			lf_node_cmp_t cmp);
void lf_list_insert(struct lf_list *lst, struct lf_node *key,
			lf_node_cmp_t cmp);
int lf_list_remove(struct lf_list *lst, const struct lf_node *key,
			lf_node_cmp_t cmp);

#endif /*__LOCK_FREE_LIST_H__*/
