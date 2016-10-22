#include <condition.h>
#include <scheduler.h>
#include <thread.h>
#include <mutex.h>
#include <ints.h>


struct condition_wait {
	struct list_head ll;
	struct thread *thread;
};

void condition_init(struct condition *condition)
{
	spin_lock_init(&condition->lock);
	list_init(&condition->wait_list);
}

void condition_wait(struct mutex *mutex, struct condition *condition)
{
	const unsigned long flags = local_int_save();
	struct thread *self = thread_current();
	struct condition_wait wait;

	wait.thread = self;
	list_add_tail(&wait.ll, &condition->wait_list);
	thread_set_state(self, THREAD_BLOCKED);

	mutex_unlock(mutex);
	local_int_restore(flags);

	if (thread_get_state(self) != THREAD_ACTIVE)
		schedule();

	mutex_lock(mutex);
}

void condition_notify(struct condition *condition)
{
	if (list_empty(&condition->wait_list))
		return;

	struct condition_wait *wait = CONTAINER_OF(condition->wait_list.next,
				struct condition_wait, ll);

	list_del(&wait->ll);
	thread_set_state(wait->thread, THREAD_ACTIVE);
}

void condition_notify_all(struct condition *condition)
{
	if (list_empty(&condition->wait_list))
		return;

	struct list_head wait_list;
	struct list_head *head;
	struct list_head *ptr;

	list_init(&wait_list);
	list_splice(&condition->wait_list, &wait_list);

	head = &wait_list;
	ptr = head->next;

	while (ptr != head) {
		struct condition_wait *wait = CONTAINER_OF(ptr,
					struct condition_wait, ll);

		ptr = ptr->next;
		thread_set_state(wait->thread, THREAD_ACTIVE);
	}
}
