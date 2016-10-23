#include <scheduler.h>
#include <thread.h>
#include <mutex.h>


struct mutex_wait {
	struct list_head ll;
	struct thread *thread;
};

void mutex_init(struct mutex *mutex)
{
	spin_lock_init(&mutex->lock);
	list_init(&mutex->wait_list);
	mutex->owner = 0;
}

void mutex_lock(struct mutex *mutex)
{
	struct thread *self = thread_current();
	struct mutex_wait wait;

	spin_lock(&mutex->lock);
	if (mutex->owner) {
		wait.thread = self;
		list_add_tail(&wait.ll, &mutex->wait_list);
		thread_set_state(self, THREAD_BLOCKED);
	} else {
		mutex->owner = self;
	}
	spin_unlock(&mutex->lock);

	if (mutex->owner == self)
		return;
	schedule();
}

void mutex_unlock(struct mutex *mutex)
{
	struct list_head *head;
	struct list_head *next;

	spin_lock(&mutex->lock);
	head = &mutex->wait_list;
	next = head->next;
	if (next != head) {
		struct mutex_wait *wait = CONTAINER_OF(next,
					struct mutex_wait, ll);
		struct thread *thread = wait->thread;

		list_del(next);
		mutex->owner = thread;
		thread_activate(wait->thread);
	} else {
		mutex->owner = 0;
	}
	spin_unlock(&mutex->lock);
}
