#include <rbtree.h>

#define RED   0
#define BLACK 1

static int rb_color(const struct rb_node *node)
{ return node->parent & 1; }

static void rb_set_color(struct rb_node *node, int color)
{ node->parent = (node->parent & (~(uintptr_t)1)) | color; }

static void rb_set_red(struct rb_node *node)
{ rb_set_color(node, RED); }

static void rb_set_black(struct rb_node *node)
{ rb_set_color(node, BLACK); }

static struct rb_node *rb_parent(const struct rb_node *node)
{ return (struct rb_node *)(node->parent & (~(uintptr_t)1)); }

static void rb_set_parent(struct rb_node *child, struct rb_node *parent)
{ child->parent = (child->parent & 1) | (uintptr_t)parent; }

static int rb_red(const struct rb_node *node)
{ return node && rb_color(node) == RED; }

static int rb_black(const struct rb_node *node)
{ return !rb_red(node); }

static void rb_rotate_left(struct rb_node *x, struct rb_tree *tree)
{
	struct rb_node *p = rb_parent(x);
	struct rb_node *r = x->right;

	x->right = r->left;
	if (x->right)
		rb_set_parent(x->right, x);
	r->left = x;
	rb_set_parent(r, p);

	if (p)
		if (p->left == x)
			p->left = r;
		else
			p->right = r;
	else
		tree->root = r;
	rb_set_parent(x, r);
}

static void rb_rotate_right(struct rb_node *x, struct rb_tree *tree)
{
	struct rb_node *p = rb_parent(x);
	struct rb_node *l = x->left;

	x->left = l->right;
	if (x->left)
		rb_set_parent(x->left, x);
	l->right = x;
	rb_set_parent(l, p);

	if (p)
		if (p->left == x)
			p->left = l;
		else
			p->right = l;
	else
		tree->root = l;
	rb_set_parent(x, l);
}

static struct rb_node *__rb_rightmost(const struct rb_node *node)
{
	if (!node)
		return 0;

	while (node->right)
		node = node->right;
	return (struct rb_node *)node;
}

static struct rb_node *__rb_leftmost(const struct rb_node *node)
{
	if (!node)
		return 0;

	while (node->left)
		node = node->left;
	return (struct rb_node *)node;
}

struct rb_node *rb_leftmost(const struct rb_tree *tree)
{
	return __rb_leftmost(tree->root);
}

struct rb_node *rb_rightmost(const struct rb_tree *tree)
{
	return __rb_rightmost(tree->root);
}

struct rb_node *rb_next(const struct rb_node *node)
{
	if (!node)
		return 0;

	if (node->right)
		return __rb_leftmost(node->right);

	const struct rb_node *p = rb_parent(node);

	while (p && p->right == node) {
		node = p;
		p = rb_parent(p);
	}

	return (struct rb_node *)p;
}

struct rb_node *rb_prev(const struct rb_node *node)
{
	if (!node)
		return 0;

	if (node->left)
		return __rb_rightmost(node->left);

	const struct rb_node *p = rb_parent(node);

	while (p && p->left == node) {
		node = p;
		p = rb_parent(p);
	}

	return (struct rb_node *)p;
}

void rb_insert(struct rb_node *node, struct rb_tree *tree)
{
	struct rb_node *p = rb_parent(node);

	while (rb_red(p)) {
		struct rb_node *g = rb_parent(p);

		if (p == g->left) {
			struct rb_node *r = g->right;

			if (rb_red(r)) {
				rb_set_red(g);
				rb_set_black(p);
				rb_set_black(r);
				node = g;
				p = rb_parent(node);
				continue;
			}

			if (node == p->right) {
				rb_rotate_left(p, tree);
				p = node;
			}
			rb_rotate_right(g, tree);
			rb_set_black(p);
			rb_set_red(g);
			break;
		} else {
			struct rb_node *l = g->left;

			if (rb_red(l)) {
				rb_set_red(g);
				rb_set_black(p);
				rb_set_black(l);
				node = g;
				p = rb_parent(node);
				continue;
			}

			if (node == p->left) {
				rb_rotate_right(p, tree);
				p = node;
			}
			rb_rotate_left(g, tree);
			rb_set_black(p);
			rb_set_red(g);
			break;
		}
	}
	rb_set_black(tree->root);
}

static void rb_erase_fix(struct rb_node *child, struct rb_node *parent,
			struct rb_tree *tree)
{
	while (rb_black(child) && child != tree->root) {
		if (child == parent->left) {
			struct rb_node *b = parent->right;

			if (rb_red(b)) {
				rb_set_black(b);
				rb_set_red(parent);
				rb_rotate_left(parent, tree);
				b = parent->right;
			}

			if (rb_black(b->left) && rb_black(b->right)) {
				rb_set_red(b);
				child = parent;
				parent = rb_parent(parent);
			} else {
				if (rb_black(b->right)) {
					rb_set_black(b->left);
					rb_set_red(b);
					rb_rotate_right(b, tree);
					b = parent->right;
				}
				rb_set_color(b, rb_color(parent));
				rb_set_black(parent);
				if (b->right)
					rb_set_black(b->right);
				rb_rotate_left(parent, tree);
				child = tree->root;
				break;
			}
		} else {
			struct rb_node *b = parent->left;

			if (rb_red(b)) {
				rb_set_black(b);
				rb_set_red(parent);
				rb_rotate_right(parent, tree);
				b = parent->left;
			}

			if (rb_black(b->right) && rb_black(b->left)) {
				rb_set_red(b);
				child = parent;
				parent = rb_parent(parent);
			} else {
				if (rb_black(b->left)) {
					rb_set_black(b->right);
					rb_set_red(b);
					rb_rotate_left(b, tree);
					b = parent->left;
				}
				rb_set_color(b, rb_color(parent));
				rb_set_black(parent);
				if (b->left)
					rb_set_black(b->left);
				rb_rotate_right(parent, tree);
				child = tree->root;
				break;
			}
		}
	}

	if (child)
		rb_set_black(child);
}

void rb_erase(struct rb_node *node, struct rb_tree *tree)
{
	struct rb_node *p, *c, *x;
	int color;

	if (node->left && node->right) {
		x = rb_next(node);
		c = x->right;
		color = rb_color(x);

		if (x == node->right) {
			p = x;
			x->left = node->left;
			x->parent = node->parent;
			rb_set_parent(x->left, x);
		} else {
			p = rb_parent(x);
			*x = *node;

			p->left = c;
			if (c)
				rb_set_parent(c, p);

			rb_set_parent(x->left, x);
			rb_set_parent(x->right, x);
		}
	} else {
		p = rb_parent(node);
		color = rb_color(node);
		x = c = node->left ? node->left : node->right;

		if (c)
			rb_set_parent(c, p);
	}

	if (rb_parent(node)) {
		if (rb_parent(node)->left == node)
			rb_parent(node)->left = x;
		else
			rb_parent(node)->right = x;
	} else
		tree->root = x;

	if (color == BLACK)
		rb_erase_fix(c, p, tree);
}
