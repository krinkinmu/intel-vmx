#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#include <stdatomic.h>

struct spinlock {
	atomic_uint next;
	atomic_uint current;
};

void spin_lock_init(struct spinlock *lock);
unsigned long spin_lock_save(struct spinlock *lock);
void spin_unlock_restore(struct spinlock *lock, unsigned long flags);


struct rwlock {
	atomic_uint write;
	atomic_uint read;
	atomic_uint next;
};

void rwlock_init(struct rwlock *lock);
unsigned long read_lock_save(struct rwlock *lock);
unsigned long write_lock_save(struct rwlock *lock);
void read_unlock_restore(struct rwlock *lock, unsigned long flags);
void write_unlock_restore(struct rwlock *lock, unsigned long flags);

#endif /*__SPINLOCK_H__*/
