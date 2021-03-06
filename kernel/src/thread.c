#include <scheduler.h>
#include <thread.h>
#include <stddef.h>
#include <stdint.h>
#include <percpu.h>
#include <debug.h>
#include <alloc.h>
#include <time.h>
#include <cpu.h>
#include <fpu.h>

struct thread_switch_frame {
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t rbx;
	uint64_t rbp;
	uint64_t rflags;
	uint64_t rip;
} __attribute__((packed));

static struct mem_cache thread_cache;
static __percpu struct thread *current;


static struct thread *thread_alloc(void)
{
	return mem_cache_alloc(&thread_cache, PA_ANY);
}

static void thread_free(struct thread *thread)
{
	mem_cache_free(&thread_cache, thread);
}

void threads_setup(void)
{
	const size_t size = sizeof(struct thread);
	const size_t align = 64;

	mem_cache_setup(&thread_cache, size, align);
}

void threads_cpu_setup(void)
{
	struct thread *thread = thread_alloc();
	uint64_t rsp;

	BUG_ON(!thread);

	/* TODO: need to think about better way to specify bootstrap stack
	 * addr and size, we need single source of truth. */
	__asm__ volatile ("movq %%rsp, %0" : "=rm"(rsp));
	thread->stack_addr = rsp & ~((uint64_t)PAGE_SIZE - 1);
	thread->stack_order = 0;
	thread->timestamp = current_time();
	thread_set_state(thread, THREAD_ACTIVE);

	BUG_ON(!(thread->fpu_state = mem_alloc(fpu_state_size())));
	fpu_state_setup(thread->fpu_state);

	current = thread;
}

static void place_thread(struct thread *next)
{
	struct thread *prev = thread_current();

	current = next;
	if (thread_get_state(prev) == THREAD_FINISHING)
		thread_set_state(prev, THREAD_FINISHED);
}

void thread_entry(struct thread *thread, thread_fptr_t fptr, void *arg)
{
	place_thread(thread);
	local_int_enable();
	fptr(arg);

	thread_set_state(thread_current(), THREAD_FINISHING);
	while (1)
		schedule();
}

struct thread *__thread_create(thread_fptr_t fptr, void *arg, int stack_order)
{
	void __thread_entry(void);

	const uintptr_t stack_addr = page_alloc(stack_order, PA_ANY);
	const size_t stack_size = (size_t)1 << (PAGE_SHIFT + stack_order);

	if (!stack_addr)
		return 0;

	struct thread *thread = thread_alloc();

	if (!thread) {
		page_free(stack_addr, stack_order);
		return 0;
	}

	thread->fpu_state = mem_alloc(fpu_state_size());
	if (!thread->fpu_state) {
		page_free(stack_addr, stack_order);
		thread_free(thread);
		return 0;
	}
	fpu_state_setup(thread->fpu_state);

	struct thread_switch_frame *frame;

	thread->stack_addr = stack_addr;
	thread->stack_order = stack_order;
	thread->stack_ptr = stack_addr + stack_size - sizeof(*frame);
	thread_set_state(thread, THREAD_BLOCKED);

	frame = (struct thread_switch_frame *)thread->stack_ptr;
	frame->r12 = (uintptr_t)thread;
	frame->r13 = (uintptr_t)fptr;
	frame->r14 = (uintptr_t)arg;
	frame->rbp = 0;
	frame->rflags = RFLAGS_RESERVED;
	frame->rip = (uintptr_t)__thread_entry;

	return thread;
}

struct thread *thread_create(thread_fptr_t fptr, void *arg)
{
	const int default_stack_order = 0;

	return __thread_create(fptr, arg, default_stack_order);
}

void thread_join(struct thread *thread)
{
	while (thread_get_state(thread) != THREAD_FINISHED)
		schedule();
}

void thread_destroy(struct thread *thread)
{
	mem_free(thread->fpu_state);
	page_free(thread->stack_addr, thread->stack_order);
	thread_free(thread);
}

struct thread *thread_current(void)
{
	return current;
}

void thread_activate(struct thread *thread)
{
	thread_set_state(thread, THREAD_ACTIVE);
	scheduler_activate_thread(thread);
}

void thread_set_state(struct thread *thread, int state)
{
	atomic_store_explicit(&thread->state, state, memory_order_relaxed);
}

int thread_get_state(struct thread *thread)
{
	return atomic_load_explicit(&thread->state, memory_order_relaxed);
}

void thread_switch_to(struct thread *next)
{
	void __thread_switch(uintptr_t *prev_state, uintptr_t next_state);

	const unsigned long flags = local_int_save();
	struct thread *prev = thread_current();

	fpu_state_save(prev->fpu_state);
	__thread_switch(&prev->stack_ptr, next->stack_ptr);
	fpu_state_restore(prev->fpu_state);
	place_thread(prev);

	local_int_restore(flags);
}
