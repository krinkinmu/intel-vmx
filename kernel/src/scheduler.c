#include <scheduler.h>
#include <spinlock.h>
#include <thread.h>
#include <percpu.h>
#include <debug.h>
#include <apic.h>
#include <time.h>
#include <list.h>
#include <cpu.h>

#define SCHEDULER_SLICE	100

struct scheduler_queue {
	struct list_head ll;
	struct spinlock lock;
	struct list_head threads;
	int cpu_id;
};

static __percpu struct scheduler_queue cpu_queue;
static __percpu struct thread *cpu_idle;
static struct scheduler_queue global_queue;
static struct rwlock queues_lock;
static struct list_head queues;
static struct spinlock print_lock;


static void scheduler_queue_setup(struct scheduler_queue *queue)
{
	const unsigned long flags = write_lock_save(&queues_lock);

	spin_lock_init(&queue->lock);
	list_init(&queue->threads);
	queue->cpu_id = local_apic_id();
	list_add_tail(&queue->ll, &queues);
	write_unlock_restore(&queues_lock, flags);
}

static struct thread *__scheduler_queue_next(struct scheduler_queue *queue)
{
	struct list_head *head = &queue->threads;

	if (list_empty(head))
		return 0;

	struct thread *thread = LIST_ENTRY(list_first(head), struct thread, ll);

	list_del(&thread->ll);
	return thread;
}

static struct thread *scheduler_queue_next(struct scheduler_queue *queue)
{
	const unsigned long flags = spin_lock_save(&queue->lock);
	struct thread *thread = __scheduler_queue_next(queue);

	spin_unlock_restore(&queue->lock, flags);
	return thread;
}

static void scheduler_queue_insert(struct scheduler_queue *queue,
			struct thread *thread)
{
	const unsigned long flags = spin_lock_save(&queue->lock);

	list_add_tail(&thread->ll, &queue->threads);
	spin_unlock_restore(&queue->lock, flags);
}

static int scheduler_need_preemption(struct thread *thread)
{
	const unsigned long long time = current_time();

	BUG_ON(time < thread->timestamp);

	return thread == cpu_idle ||
		(thread_get_state(thread) != THREAD_ACTIVE) ||
		(time - thread->timestamp > SCHEDULER_SLICE);
}

static struct thread *__scheduler_next_thread(void)
{
	struct thread *next = scheduler_queue_next(&cpu_queue);

	if (next)
		return next;

	next = scheduler_queue_next(&global_queue);
	if (next)
		return next;

	/* TODO: think about avoiding this lock, it's really annoying */
	const unsigned long flags = read_lock_save(&queues_lock);
	struct list_head *head = &cpu_queue.ll;
	struct list_head *ptr = head->next;

	for (; ptr != head; ptr = ptr->next) {
		if (ptr == &queues)
			continue;

		struct scheduler_queue *queue = LIST_ENTRY(ptr,
					struct scheduler_queue, ll);

		if ((next = scheduler_queue_next(queue)))
			break;
	}	
	read_unlock_restore(&queues_lock, flags);

	if (next)
		return next;

	struct thread *current = thread_current();

	if (thread_get_state(current) == THREAD_ACTIVE)
		return 0;

	return cpu_idle;
}

static void scheduler_preempt_thread(struct thread *prev)
{
	const int state = thread_get_state(prev);

	if (prev == cpu_idle || state != THREAD_ACTIVE) {
		if (state == THREAD_FINISHING)
			thread_set_state(prev, THREAD_FINISHED);
		return;
	}

	const unsigned long flags = spin_lock_save(&cpu_queue.lock);

	list_add_tail(&prev->ll, &cpu_queue.threads);
	spin_unlock_restore(&cpu_queue.lock, flags);
}

static struct thread *scheduler_next_thread(void)
{
	struct thread *prev = thread_current();
	struct thread *next;

	if (!scheduler_need_preemption(prev))
		return 0;

	if ((next = __scheduler_next_thread())) {
		scheduler_preempt_thread(prev);
		next->timestamp = current_time();
	}

	return next;
}

void scheduler_activate_thread(struct thread *thread)
{
	thread_set_state(thread, THREAD_ACTIVE);
	scheduler_queue_insert(&cpu_queue, thread);
}

void scheduler_block_thread(void)
{
	struct thread *thread = thread_current();

	thread_set_state(thread, THREAD_BLOCKED);
}

void schedule(void)
{
	const unsigned long flags = local_int_save();
	struct thread *next = scheduler_next_thread();

	if (next)
		thread_switch_to(next);
	local_int_restore(flags);
}

void scheduler_setup(void)
{
	rwlock_init(&queues_lock);
	list_init(&queues);
	spin_lock_init(&print_lock);
	scheduler_queue_setup(&global_queue);
}

static void scheduler_cpu_idle(void *unused)
{
	(void) unused;

	while (1)
		schedule();
}

void scheduler_cpu_setup(void)
{
	BUG_ON(!(cpu_idle = thread_create(&scheduler_cpu_idle, 0)));
	thread_set_state(cpu_idle, THREAD_ACTIVE);
	scheduler_queue_setup(&cpu_queue);
}
