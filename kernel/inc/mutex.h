#ifndef __MUTEX_H__
#define __MUTEX_H__

#include <spinlock.h>
#include <list.h>


struct thread;

struct mutex {
	struct spinlock lock;
	struct list_head wait_list;
	struct thread *owner;
};

void mutex_init(struct mutex *mutex);
void mutex_lock(struct mutex *mutex);
void mutex_unlock(struct mutex *mutex);

#endif /*__MUTEX_H__*/
