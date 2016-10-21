#ifndef __LOCK_FREE_LIST_H__
#define __LOCK_FREE_LIST_H__


struct lf_list_head {
	struct lf_list_head * _Atomic next;
};

typedef int (*lf_node_cmp_t)(const struct lf_list_head *,
			const struct lf_list_head *);


void lf_list_init(struct lf_list_head *lst);
struct lf_list_head *lf_list_lookup(struct lf_list_head *lst,
			const struct lf_list_head *key, lf_node_cmp_t cmp);
int lf_list_insert(struct lf_list_head *lst, struct lf_list_head *key,
			lf_node_cmp_t cmp);
int lf_list_remove(struct lf_list_head *lst, const struct lf_list_head *key,
			lf_node_cmp_t cmp);

#endif /*__LOCK_FREE_LIST_H__*/
