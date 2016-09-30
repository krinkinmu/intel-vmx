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
#include <scheduler.h>
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

void main(const struct mboot_info *info)
{
	gdb_hang();
	uart8250_setup();
	balloc_setup(info);
	page_alloc_setup();
	mem_alloc_setup();
	threads_setup();
	acpi_early_setup();

	apic_setup();
	ints_setup();
	time_setup();
	scheduler_setup();
	smp_early_setup();

	percpu_cpu_setup();
	threads_cpu_setup();
	gdt_cpu_setup();
	scheduler_cpu_setup();
	ints_cpu_setup();
	time_cpu_setup();
	local_int_enable();

	smp_setup();
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
