#include <acpi.h>
#include <uart8250.h>
#include <balloc.h>
#include <debug.h>
#include <time.h>
#include <apic.h>
#include <ints.h>
#include <cpu.h>
#include <smpboot.h>
#include <memory.h>
#include <alloc.h>
#include <percpu.h>
#include <thread.h>
#include <paging.h>
#include <scheduler.h>
#include <fpu.h>
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

static void thread_function(void *id)
{
	printf("%d on %d\n", (int)(uintptr_t)id, local_apic_id());
	while (1) {
		printf("%d, hello from %d\n", (int)(uintptr_t)id, local_apic_id());
		udelay(10000);
	}
}

static int int_less(const void *lptr, const void *rptr)
{
	const int l = *((const int *)lptr);
	const int r = *((const int *)rptr);

	if (l != r)
		return l < r ? -1 : 1;
	return 0;
}

static void test_sort(void)
{
	const int sorted[] = {-42, -42, -23, -7, -1, -1, 13, 23, 42, 42, 56, 77, 88};
	int array[]        = {-1, 23, 42, -42, 56, 88, -23, 42, -42, -1, 77, 13, -7};

	qsort(array, sizeof(array)/sizeof(array[0]), sizeof(array[0]), &int_less);
	for (int i = 0; i != sizeof(array)/sizeof(array[0]); ++i) {
		printf("%d\n", array[i]);
		BUG_ON(array[i] != sorted[i]);
	}
}

void main(const struct mboot_info *info)
{
	gdb_hang();
	uart8250_setup();
	test_sort();
	ints_early_setup();
	acpi_early_setup();

	balloc_setup(info);
	page_alloc_setup();
	mem_alloc_setup();
	threads_setup();

	paging_setup();
	apic_setup();
	time_setup();
	scheduler_setup();
	smp_setup();

	paging_cpu_setup();
	fpu_cpu_setup();
	gdt_cpu_setup();
	percpu_cpu_setup();
	threads_cpu_setup();
	scheduler_cpu_setup();
	ints_cpu_setup();
	time_cpu_setup();
	local_int_enable();

	//vmx_setup();

	struct thread *thread0 = thread_create(&thread_function, (void *)0);
	struct thread *thread1 = thread_create(&thread_function, (void *)1);
	struct thread *thread2 = thread_create(&thread_function, (void *)2);
	struct thread *thread3 = thread_create(&thread_function, (void *)3);

	printf("thread0 %p\n", thread0);
	printf("thread1 %p\n", thread1);
	printf("thread2 %p\n", thread2);
	printf("thread3 %p\n", thread3);

	thread_activate(thread0);
	thread_activate(thread1);
	thread_activate(thread2);
	thread_activate(thread3);

	while (1);
}
