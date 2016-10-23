#ifndef __CONDITION_H__
#define __CONDITION_H__

#include <list.h>

struct condition {
	struct list_head wait_list;
};

struct mutex;

void condition_init(struct condition *condition);
void condition_wait(struct mutex *mutex, struct condition *condition);
void condition_notify(struct condition *condition);
void condition_notify_all(struct condition *condition);

#endif /*__CONDITION_H__*/
