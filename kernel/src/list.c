#include <list.h>

void list_init(struct list_head *head)
{ head->next = head->prev = head; }

static void list_insert(struct list_head *new, struct list_head *prev,
			struct list_head *next)
{
	new->prev = prev;
	new->next = next;
	prev->next = new;
	next->prev = new;
}

void list_add(struct list_head *new, struct list_head *head)
{ list_insert(new, head, head->next); }

void list_add_tail(struct list_head *new, struct list_head *head)
{ list_insert(new, head->prev, head); }

static void __list_del(struct list_head *prev, struct list_head *next)
{
	prev->next = next;
	next->prev = prev;
}

void list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	entry->next = 0;
	entry->prev = 0;
}

static void __list_splice(struct list_head *list, struct list_head *prev,
			struct list_head *next)
{
	struct list_head *first = list->next;
	struct list_head *last = list->prev;

	first->prev = prev;
	prev->next = first;
	last->next = next;
	next->prev = last;
}

void list_splice(struct list_head *list, struct list_head *head)
{
	if (!list_empty(list)) {
		__list_splice(list, head, head->next);
		list_init(list);
	}
}

int list_empty(const struct list_head *head)
{ return head->next == head; }

struct list_head *list_first(struct list_head *head)
{ return head->next; }

size_t list_size(const struct list_head *head)
{
	size_t size = 0;

	for (const struct list_head *pos = head->next; pos != head;
		pos = pos->next)
		++size;
	return size;
}
