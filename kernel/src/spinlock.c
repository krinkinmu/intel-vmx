#include <spinlock.h>
#include <ints.h>
#include <cpu.h>

static void __spin_lock(struct spinlock *lock)
{
	const unsigned ticket = atomic_fetch_add_explicit(&lock->next, 1,
				memory_order_relaxed);

	while (atomic_load_explicit(&lock->current, memory_order_acquire)
				!= ticket)
		cpu_relax();
}

static void __spin_unlock(struct spinlock *lock)
{
	atomic_store_explicit(&lock->current, lock->current + 1,
				memory_order_release);
}

void spin_lock_init(struct spinlock *lock)
{
	atomic_init(&lock->next, 0);
	atomic_init(&lock->current, 0);
}

unsigned long spin_lock_save(struct spinlock *lock)
{
	const unsigned long flags = rflags();

	local_int_disable();
	__spin_lock(lock);
	return flags;
}

void spin_unlock_restore(struct spinlock *lock, unsigned long flags)
{
	__spin_unlock(lock);
	if (flags & RFLAGS_IF)
		local_int_enable();
}
