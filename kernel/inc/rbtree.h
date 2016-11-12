#ifndef __RBTREE_H__
#define __RBTREE_H__

#include <stdint.h>
#include <kernel.h>

struct rb_node {
	struct rb_node *left;
	struct rb_node *right;
	uintptr_t parent;
};

struct rb_tree {
	struct rb_node *root;
};

#define TREE_ENTRY(ptr, type, member) CONTAINER_OF(ptr, type, member)

static inline void rb_link(struct rb_node *node, struct rb_node *parent,
			struct rb_node **plink)
{
	node->parent = (uintptr_t)parent;
	node->left = node->right = 0;
	*plink = node;
}

struct rb_node *rb_rightmost(const struct rb_tree *tree);
struct rb_node *rb_leftmost(const struct rb_tree *tree);
struct rb_node *rb_next(const struct rb_node *node);
struct rb_node *rb_prev(const struct rb_node *node);
void rb_erase(struct rb_node *node, struct rb_tree *tree);
void rb_insert(struct rb_node *node, struct rb_tree *tree);

#endif /*__RBTREE_H__*/
