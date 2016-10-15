#include <spinlock.h>
#include <percpu.h>
#include <ints.h>
#include <time.h>
#include <rcu.h>
#include <cpu.h>


static __percpu unsigned long rcu_cpu_qs;
static __percpu unsigned long rcu_last_cpu_qs;
static __percpu unsigned long rcu_cpu_gen;
static __percpu unsigned long rcu_cpu_flags;
static __percpu unsigned long rcu_cpu_depth;

static __percpu struct list_head rcu_cpu_next;
static __percpu struct list_head rcu_cpu_curr;

static struct spinlock rcu_lock;
static unsigned long rcu_cpumask;
static unsigned long rcu_gen;


static void rcu_reset_cpumask(void)
{
	for (int i = 0; i != cpu_count(); ++i)
		rcu_cpumask |= 1ul << i;
}

static void __rcu_check_qs(void)
{
	const unsigned long mask = 1ul << cpu_id();

	if ((rcu_cpumask & mask) == 0)
		return;

	rcu_cpumask &= ~mask;
	if (!rcu_cpumask)
		return;

	rcu_reset_cpumask();
	++rcu_gen;
}

static void rcu_cpu_release(struct list_head *head)
{
	struct list_head *ptr = head->next;

	while (ptr != head) {
		struct rcu_callback *cb = LIST_ENTRY(ptr,
					struct rcu_callback, ll);

		ptr = ptr->next;
		cb->callback(cb);
	}
}

static void rcu_cpu_check_qs(void)
{
	if (rcu_last_cpu_qs == rcu_cpu_qs)
		return;

	unsigned long flags = spin_lock_save(&rcu_lock);
	int advance = 0;

	__rcu_check_qs();
	if (rcu_cpu_gen != rcu_gen) {
		rcu_last_cpu_qs = rcu_cpu_qs;
		advance = 1;
	}
	rcu_cpu_gen = rcu_gen;
	spin_unlock_restore(&rcu_lock, flags);

	if (!advance)
		return;

	struct list_head done;

	flags = local_int_save();
	list_init(&done);
	list_splice(&rcu_cpu_curr, &done);
	list_splice(&rcu_cpu_next, &rcu_cpu_curr);
	local_int_restore(flags);

	rcu_cpu_release(&done);
}

void rcu_tick(void)
{
	const int turn = current_time() % cpu_count();

	if (turn == cpu_id())
		rcu_cpu_check_qs();
}

void rcu_report_qs(void)
{
	++rcu_cpu_qs;
}

void rcu_read_lock(void)
{
	const unsigned long flags = local_int_save();

	if (!(rcu_cpu_depth++))
		rcu_cpu_flags = flags;
}

void rcu_read_unlock(void)
{
	if (!(--rcu_cpu_depth))
		local_int_restore(rcu_cpu_flags);
}

void rcu_call(struct rcu_callback *cb, void (*fptr)(struct rcu_callback *))
{
	cb->callback = fptr;

	const unsigned long flags = local_int_save();

	list_add(&cb->ll, &rcu_cpu_next);
	local_int_restore(flags);
}

void rcu_setup(void)
{
	spin_lock_init(&rcu_lock);
	rcu_reset_cpumask();
}

void rcu_cpu_setup(void)
{
	list_init(&rcu_cpu_next);
	list_init(&rcu_cpu_curr);
}
