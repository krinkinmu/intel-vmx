#include <scheduler.h>
#include <spinlock.h>
#include <ints.h>
#include <cpu.h>

static void __spin_lock(struct spinlock *lock)
{
	const unsigned ticket = atomic_fetch_add_explicit(&lock->next, 1,
				memory_order_relaxed);

	while (atomic_load_explicit(&lock->current, memory_order_relaxed)
				!= ticket)
		cpu_relax();
	atomic_thread_fence(memory_order_acquire);
}

static void __spin_unlock(struct spinlock *lock)
{
	const unsigned current = atomic_load_explicit(&lock->current,
				memory_order_relaxed);

	atomic_thread_fence(memory_order_release);
	atomic_store_explicit(&lock->current, current + 1,
				memory_order_relaxed);
}

void spin_lock_init(struct spinlock *lock)
{
	atomic_init(&lock->next, 0);
	atomic_init(&lock->current, 0);
}

void spin_lock(struct spinlock *lock)
{
	preempt_disable();
	__spin_lock(lock);
}

unsigned long spin_lock_save(struct spinlock *lock)
{
	const unsigned long flags = local_int_save();

	spin_lock(lock);
	return flags;
}

void spin_unlock(struct spinlock *lock)
{
	__spin_unlock(lock);
	preempt_enable();
}

void spin_unlock_restore(struct spinlock *lock, unsigned long flags)
{
	spin_unlock(lock);
	local_int_restore(flags);
}

void rwlock_init(struct rwlock *lock)
{
	atomic_init(&lock->write, 0);
	atomic_init(&lock->read, 0);
	atomic_init(&lock->next, 0);
}

static void __read_lock(struct rwlock *lock)
{
	const unsigned ticket = atomic_fetch_add_explicit(&lock->next, 1,
				memory_order_relaxed);

	while (atomic_load_explicit(&lock->read, memory_order_relaxed)
				!= ticket)
		cpu_relax();
	atomic_store_explicit(&lock->read, ticket + 1, memory_order_relaxed);
	atomic_thread_fence(memory_order_acquire);
}

static void __read_unlock(struct rwlock *lock)
{
	atomic_thread_fence(memory_order_release);
	atomic_fetch_add_explicit(&lock->write, 1, memory_order_relaxed);
}

static void __write_lock(struct rwlock *lock)
{
	const unsigned ticket = atomic_fetch_add_explicit(&lock->next, 1,
				memory_order_relaxed);

	while (atomic_load_explicit(&lock->write, memory_order_relaxed)
				!= ticket)
		cpu_relax();
	atomic_thread_fence(memory_order_acquire);
}

static void __write_unlock(struct rwlock *lock)
{
	const unsigned read = atomic_load_explicit(&lock->read,
				memory_order_relaxed);
	const unsigned write = atomic_load_explicit(&lock->write,
				memory_order_relaxed);

	atomic_thread_fence(memory_order_release);
	atomic_store_explicit(&lock->read, read + 1,
				memory_order_relaxed);
	atomic_store_explicit(&lock->write, write + 1,
				memory_order_relaxed);
}

void read_lock(struct rwlock *lock)
{
	preempt_disable();
	__read_lock(lock);
}

unsigned long read_lock_save(struct rwlock *lock)
{
	const unsigned long flags = local_int_save();

	read_lock(lock);
	return flags;
}

void read_unlock(struct rwlock *lock)
{
	__read_unlock(lock);
	preempt_enable();
}

void read_unlock_restore(struct rwlock *lock, unsigned long flags)
{
	read_unlock(lock);
	local_int_restore(flags);
}

void write_lock(struct rwlock *lock)
{
	preempt_disable();
	__write_lock(lock);
}

unsigned long write_lock_save(struct rwlock *lock)
{
	const unsigned long flags = local_int_save();

	write_lock(lock);
	return flags;
}

void write_unlock(struct rwlock *lock)
{
	__write_unlock(lock);
	preempt_enable();
}

void write_unlock_restore(struct rwlock *lock, unsigned long flags)
{
	write_unlock(lock);
	local_int_restore(flags);
}
