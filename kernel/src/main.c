#include <scheduler.h>
#include <hashtable.h>
#include <uart8250.h>
#include <smpboot.h>
#include <balloc.h>
#include <memory.h>
#include <percpu.h>
#include <thread.h>
#include <paging.h>
#include <hazptr.h>
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

struct ht_int {
	struct hash_node hn;
	int value;
};

static int ht_int_equal(const struct hash_node *node, const void *key)
{
	const struct ht_int *l = (const struct ht_int *)node;
	const struct ht_int *r = key;

	return l->value == r->value;
}

static void __test_hashtable(void *unused)
{
	struct mem_cache cache;
	struct hash_table ht;
	int i, j;

	(void) unused;

	hash_setup(&ht);
	mem_cache_setup(&cache, sizeof(struct ht_int), sizeof(struct ht_int));

	for (i = 0; i != 1000000; ++i) {
		struct ht_int *node = mem_cache_alloc(&cache, PA_ANY);

		if (!node)
			break;
		node->value = i;
		BUG_ON(hash_insert(&ht, (uint64_t)i, &node->hn,
					&ht_int_equal) != &node->hn);
	}

	for (j = 0; j != i; ++j) {
		struct hash_node *node;
		struct ht_int key;

		key.value = j;

		BUG_ON(!(node = hash_lookup(&ht, (uint64_t)j, &key,
					&ht_int_equal)));
		BUG_ON(hash_remove(&ht, (uint64_t)j, &key,
					&ht_int_equal) != node);
		mem_cache_free(&cache, (struct ht_int *)node);
	}

	mem_cache_release(&cache);
	hash_release(&ht);
}

static void test_hashtable(void)
{
	struct thread *thread = thread_create(&__test_hashtable, 0);

	printf("start hashtable test\n");
	thread_activate(thread);
	thread_join(thread);
	thread_destroy(thread);
	printf("finished hashtable test\n");
}

void main(const struct mboot_info *info)
{
	gdb_hang();
	uart8250_setup();
	ints_early_setup();
	acpi_early_setup();

	apic_setup();
	balloc_setup(info);
	percpu_setup();
	page_alloc_setup();
	mem_alloc_setup();
	hp_setup();
	rcu_setup();
	threads_setup();

	paging_setup();
	time_setup();
	scheduler_setup();
	smp_setup();

	cpu_setup();

	//vmx_setup();

	test_hashtable();

	while (1);
}
