#include <scheduler.h>
#include <uart8250.h>
#include <smpboot.h>
#include <balloc.h>
#include <memory.h>
#include <percpu.h>
#include <thread.h>
#include <paging.h>
#include <hazptr.h>
#include <lflist.h>
#include <debug.h>
#include <alloc.h>
#include <time.h>
#include <acpi.h>
#include <apic.h>
#include <ints.h>
#include <cpu.h>
#include <rcu.h>
#include <vmx.h>


static void acpi_early_setup(void)
{
	static ACPI_TABLE_DESC tables[16];
	static const size_t size = sizeof(tables)/sizeof(tables[0]);

	ACPI_STATUS status = AcpiInitializeTables(tables, size, FALSE);
	if (ACPI_FAILURE(status))
		BUG("Failed to initialize ACPICA tables subsytem\n");
}

static void gdb_hang(void)
{
#ifdef DEBUG
	static volatile int wait = 1;
	while (wait);
#endif
}

struct lf_int {
	struct lf_list_head ll;
	int value;
};

static int lf_int_cmp(const struct lf_list_head *l,
			const struct lf_list_head *r)
{
	const struct lf_int *left = (const struct lf_int *)l;
	const struct lf_int *right = (const struct lf_int *)r;

	if (left->value != right->value)
		return left->value < right->value ? -1 : 1;
	return 0;
}

static void __test_lflist(void *unused)
{
	struct mem_cache cache;
	struct lf_list_head lst;
	int i, j;

	(void) unused;

	lf_list_init(&lst);
	mem_cache_setup(&cache, sizeof(struct lf_int), sizeof(struct lf_int));

	for (i = 0; i != 100000; ++i) {
		struct lf_int *node = mem_cache_alloc(&cache, PA_ANY);

		if (!node)
			break;
		node->value = i;
		BUG_ON(!lf_list_insert(&lst, &node->ll, &lf_int_cmp));
	}

	for (j = 0; j != i; ++j) {
		struct lf_list_head *node;
		struct lf_int key;

		key.value = j;
		BUG_ON(!(node = lf_list_lookup(&lst, &key.ll, &lf_int_cmp)));
		BUG_ON(!lf_list_remove(&lst, &key.ll, &lf_int_cmp));
		mem_cache_free(&cache, (struct lf_int *)node);
	}

	mem_cache_release(&cache);
}

static void test_lflist(void)
{
	struct thread *thread = thread_create(&__test_lflist, 0);

	printf("start lock-free linked list test\n");
	thread_activate(thread);
	thread_join(thread);
	thread_destroy(thread);
	printf("finished lock-free linked list test\n");
}

void main(const struct mboot_info *info)
{
	gdb_hang();
	uart8250_setup();
	ints_early_setup();
	acpi_early_setup();

	balloc_setup(info);
	page_alloc_setup();
	mem_alloc_setup();
	hp_setup();
	rcu_setup();
	threads_setup();

	paging_setup();
	apic_setup();
	time_setup();
	scheduler_setup();
	smp_setup();

	cpu_setup();

	//vmx_setup();

	test_lflist();

	while (1);
}
