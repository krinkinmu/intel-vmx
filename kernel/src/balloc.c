#include <balloc.h>
#include <debug.h>


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



#define BALLOC_MAX_RANGES	128
static struct memory_node balloc_nodes[BALLOC_MAX_RANGES];
static struct list_head balloc_free_list;

struct rb_tree free_ranges;
struct rb_tree memory_map;


static struct memory_node *balloc_alloc_node(void)
{
	BUG_ON(list_empty(&balloc_free_list));

	struct list_head *node = balloc_free_list.next;

	list_del(node);
	return LIST_ENTRY(node, struct memory_node, link.ll);
}

static void balloc_free_node(struct memory_node *node)
{
	list_add(&node->link.ll, &balloc_free_list);
}

static void __balloc_add_range(struct rb_tree *tree,
			unsigned long long from, unsigned long long to)
{
	struct rb_node **plink = &tree->root;
	struct rb_node *parent = 0;

	while (*plink) {
		struct memory_node *node = TREE_ENTRY(*plink,
					struct memory_node, link.rb);

		parent = *plink;
		if (node->begin < from)
			plink = &parent->right;
		else
			plink = &parent->left;
	}

	struct memory_node *new = balloc_alloc_node();

	new->begin = from;
	new->end = to;

	rb_link(&new->link.rb, parent, plink);
	rb_insert(&new->link.rb, tree);

	struct memory_node *prev = TREE_ENTRY(rb_prev(&new->link.rb),
				struct memory_node, link.rb);

	if (prev && prev->end >= new->begin) {
		new->begin = prev->begin;
		if (prev->end > new->end)
			new->end = prev->end;
		rb_erase(&prev->link.rb, tree);
		balloc_free_node(prev);
	}

	struct memory_node *next = TREE_ENTRY(rb_next(&new->link.rb),
				struct memory_node, link.rb);

	if (next && next->begin <= new->end) {
		new->end = next->end;
		rb_erase(&next->link.rb, tree);
		balloc_free_node(next);
	}
}

static void __balloc_remove_range(struct rb_tree *tree,
			unsigned long long from, unsigned long long to)
{
	struct rb_node *link = tree->root;
	struct memory_node *ptr = 0;

	while (link) {
		struct memory_node *node = TREE_ENTRY(link,
					struct memory_node, link.rb);

		if (node->end > from) {
			link = link->left;
			ptr = node;
		} else {
			link = link->right;
		}
	}

	while (ptr && ptr->begin < to) {
		struct memory_node *next = TREE_ENTRY(rb_next(&ptr->link.rb),
					struct memory_node, link.rb);

		rb_erase(&ptr->link.rb, tree);
		if (ptr->begin < from)
			__balloc_add_range(tree, ptr->begin, from);
		if (ptr->end > to)
			__balloc_add_range(tree, to, ptr->end);
		balloc_free_node(ptr);
		ptr = next;
	}
}

uintptr_t __balloc_alloc(size_t size, uintptr_t align,
			uintptr_t from, uintptr_t to)
{
	struct rb_tree *tree = &free_ranges;
	struct rb_node *link = tree->root;
	struct memory_node *ptr = 0;

	while (link) {
		struct memory_node *node = TREE_ENTRY(link,
					struct memory_node, link.rb);

		if (node->end > from) {
			link = link->left;
			ptr = node;
		} else {
			link = link->right;
		}
	}

	while (ptr && ptr->begin < to) {
		const unsigned long long b = ptr->begin > from
					? ptr->begin : from;
		const unsigned long long e = ptr->end < to ? ptr->end : to;
		const unsigned long long mask = align - 1;
		const unsigned long long addr = (b + mask) & ~mask;

		if (addr + size <= e) {
			rb_erase(&ptr->link.rb, tree);
			if (ptr->begin < addr)
				__balloc_add_range(tree, ptr->begin, addr);
			if (ptr->end > addr + size)
				__balloc_add_range(tree, addr + size, ptr->end);
			balloc_free_node(ptr);
			return addr;
		}

		ptr = TREE_ENTRY(rb_next(&ptr->link.rb),
					struct memory_node, link.rb);
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
	__balloc_add_range(&free_ranges, begin, end);
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
		const unsigned long long rbegin = entry->addr;
		const unsigned long long rend = rbegin + entry->length;

		__balloc_add_range(&memory_map, rbegin, rend);
		ptr += entry->size + sizeof(entry->size);
	}

	extern char kernel_phys_begin[];
	extern char kernel_phys_end[];

	const uintptr_t kbegin = (uintptr_t)kernel_phys_begin;
	const uintptr_t kend = (uintptr_t)kernel_phys_end;

	__balloc_add_range(&memory_map, kbegin, kend);

	ptr = begin;
	while (ptr + sizeof(struct mboot_mmap_entry) <= end) {
		const struct mboot_mmap_entry *entry =
					(const struct mboot_mmap_entry *)ptr;
		const unsigned long long rbegin = entry->addr;
		const unsigned long long rend = rbegin + entry->length;

		if (entry->type == 1)
			__balloc_add_range(&free_ranges, rbegin, rend);
		ptr += entry->size + sizeof(entry->size);
	}

	__balloc_remove_range(&free_ranges, kbegin, kend);
}

static void __balloc_dump_ranges(const struct rb_tree *tree)
{
	const struct memory_node *node = TREE_ENTRY(rb_leftmost(tree->root),
				struct memory_node, link.rb);

	while (node) {
		printf("memory range: 0x%llx-0x%llx\n",
					(unsigned long long)node->begin,
					(unsigned long long)node->end);
		node = TREE_ENTRY(rb_next(&node->link.rb),
					struct memory_node, link.rb);
	}
}

static void balloc_dump_ranges(void)
{
	printf("known memory ranges:\n");
	__balloc_dump_ranges(&memory_map);
	printf("free memory ranges:\n");
	__balloc_dump_ranges(&free_ranges);
}

void balloc_setup(const struct mboot_info *info)
{
	balloc_setup_nodes();
	balloc_parse_mmap(info);
	balloc_dump_ranges();
}
