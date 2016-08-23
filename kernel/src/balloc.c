#include <stdint.h>
#include <balloc.h>
#include <rbtree.h>
#include <debug.h>
#include <list.h>


struct mboot_info {
	uint32_t flags;
	uint8_t ignore0[40];
	uint32_t mmap_size;
	uint32_t mmap_addr;
} __attribute__((packed));

struct mboot_mmap_entry {
	uint32_t size;
	uint64_t addr;
	uint64_t length;
	uint32_t type;
} __attribute__((packed));

struct balloc_node {
	union {
		struct rb_node rb;
		struct list_head ll;
	} link;
	uintptr_t begin;
	uintptr_t end;
};

#define BALLOC_MAX_RANGES	128
static struct balloc_node balloc_nodes[BALLOC_MAX_RANGES];
static struct list_head balloc_free_list;
static struct rb_tree balloc_free_ranges;


static struct balloc_node *balloc_alloc_node(void)
{
	BUG_ON(list_empty(&balloc_free_list));

	struct list_head *node = balloc_free_list.next;

	list_del(node);
	return LIST_ENTRY(node, struct balloc_node, link.ll);
}

static void balloc_free_node(struct balloc_node *node)
{
	list_add(&node->link.ll, &balloc_free_list);
}

static void balloc_add_range(uintptr_t from, uintptr_t to)
{
	struct rb_node **plink = &balloc_free_ranges.root;
	struct rb_node *parent = 0;

	while (*plink) {
		struct balloc_node *node = TREE_ENTRY(*plink,
					struct balloc_node, link.rb);

		parent = *plink;
		if (node->begin < from)
			plink = &parent->right;
		else
			plink = &parent->left;
	}

	struct balloc_node *new = balloc_alloc_node();

	new->begin = from;
	new->end = to;

	rb_link(&new->link.rb, parent, plink);
	rb_insert(&new->link.rb, &balloc_free_ranges);

	struct balloc_node *prev = TREE_ENTRY(rb_prev(&new->link.rb),
				struct balloc_node, link.rb);

	if (prev && prev->end >= new->begin) {
		new->begin = prev->begin;
		rb_erase(&prev->link.rb, &balloc_free_ranges);
		balloc_free_node(prev);
	}

	struct balloc_node *next = TREE_ENTRY(rb_next(&new->link.rb),
				struct balloc_node, link.rb);

	if (next && next->begin <= new->end) {
		new->end = next->end;
		rb_erase(&next->link.rb, &balloc_free_ranges);
		balloc_free_node(next);
	}
}

static void balloc_remove_range(uintptr_t from, uintptr_t to)
{
	struct rb_node *link = balloc_free_ranges.root;
	struct balloc_node *ptr = 0;

	while (link) {
		struct balloc_node *node = TREE_ENTRY(link,
					struct balloc_node, link.rb);

		if (node->end > from) {
			link = link->left;
			ptr = node;
		} else {
			link = link->right;
		}
	}

	while (ptr && ptr->begin < to) {
		struct balloc_node *next = TREE_ENTRY(rb_next(&ptr->link.rb),
					struct balloc_node, link.rb);

		rb_erase(&ptr->link.rb, &balloc_free_ranges);
		if (ptr->begin < from)
			balloc_add_range(ptr->begin, from);
		if (ptr->end > to)
			balloc_add_range(to, ptr->end);
		balloc_free_node(ptr);
		ptr = next;
	}
}

uintptr_t __balloc_alloc(size_t size, uintptr_t align,
			uintptr_t from, uintptr_t to)
{
	struct rb_node *link = balloc_free_ranges.root;
	struct balloc_node *ptr = 0;

	while (link) {
		struct balloc_node *node = TREE_ENTRY(link,
					struct balloc_node, link.rb);

		if (node->end > from) {
			link = link->left;
			ptr = node;
		} else {
			link = link->right;
		}
	}

	while (ptr && ptr->begin < to) {
		const uintptr_t b = ptr->begin > from ? ptr->begin : from;
		const uintptr_t e = ptr->end < to ? ptr->end : to;
		const uintptr_t addr = (b + (align - 1)) & ~(align - 1);

		if (addr + size <= e) {
			rb_erase(&ptr->link.rb, &balloc_free_ranges);
			if (ptr->begin < addr)
				balloc_add_range(ptr->begin, addr);
			if (ptr->end > addr + size)
				balloc_add_range(addr + size, ptr->end);
			balloc_free_node(ptr);
			return addr;
		}

		ptr = TREE_ENTRY(rb_next(&ptr->link.rb),
					struct balloc_node, link.rb);
	}

	return to;
}

uintptr_t balloc_alloc(size_t size, uintptr_t from, uintptr_t to)
{
	uintptr_t align = 32;

	if (size <= 8)
		align = 8;
	if (size <= 16)
		align = 16;
	return __balloc_alloc(size, align, from, to);
}

void balloc_free(uintptr_t begin, uintptr_t end)
{
	balloc_add_range(begin, end);
}


static void balloc_setup_nodes(void)
{
	list_init(&balloc_free_list);

	for (int i = 0; i != BALLOC_MAX_RANGES; ++i)
		balloc_free_node(&balloc_nodes[i]);
}

static void balloc_parse_mmap(const struct mboot_info *info)
{
	BUG_ON((info->flags & (1ul << 6)) == 0);

	const uintptr_t begin = info->mmap_addr;
	const uintptr_t end = begin + info->mmap_size;
	uintptr_t ptr = begin;

	while (ptr + sizeof(struct mboot_mmap_entry) <= end) {
		const struct mboot_mmap_entry *entry =
					(const struct mboot_mmap_entry *)ptr;
		const uintptr_t range_begin = entry->addr;
		const uintptr_t range_end = range_begin + entry->length;

		if (entry->type == 1)
			balloc_add_range(range_begin, range_end);
		ptr += entry->size + sizeof(entry->size);
	}

	ptr = begin;
	while (ptr + sizeof(struct mboot_mmap_entry) <= end) {
		const struct mboot_mmap_entry *entry =
					(const struct mboot_mmap_entry *)ptr;
		const uintptr_t range_begin = entry->addr;
		const uintptr_t range_end = range_begin + entry->length;

		if (entry->type != 1)
			balloc_remove_range(range_begin, range_end);
		ptr += entry->size + sizeof(entry->size);
	}

	extern char kernel_phys_begin[];
	extern char kernel_phys_end[];

	const uintptr_t kernel_begin = (uintptr_t)kernel_phys_begin;
	const uintptr_t kernel_end = (uintptr_t)kernel_phys_end;

	balloc_remove_range(kernel_begin, kernel_end);
}

static void balloc_dump_ranges(void)
{
	const struct rb_tree *tree = &balloc_free_ranges;
	struct balloc_node *node = TREE_ENTRY(rb_leftmost(tree->root),
				struct balloc_node, link.rb);

	printf("free memory ranges:\n");
	while (node) {
		printf("memory range: 0x%lx-0x%lx\n",
					(unsigned long)node->begin,
					(unsigned long)node->end);
		node = TREE_ENTRY(rb_next(&node->link.rb),
					struct balloc_node, link.rb);
	}
}

void balloc_setup(const struct mboot_info *info)
{
	balloc_setup_nodes();
	balloc_parse_mmap(info);
	balloc_dump_ranges();
}
