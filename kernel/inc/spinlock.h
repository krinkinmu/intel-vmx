#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#include <stdatomic.h>

struct spinlock {
	atomic_int next;
	atomic_int current;
};

void spin_lock_init(struct spinlock *lock);
unsigned long spin_lock_save(struct spinlock *lock);
void spin_unlock_restore(struct spinlock *lock, unsigned long flags);

#endif /*__SPINLOCK_H__*/
