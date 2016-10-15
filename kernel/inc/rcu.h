#ifndef __RCU_H__
#define __RCU_H__

#include <list.h>

struct rcu_callback {
	struct list_head ll;
	void (*callback)(struct rcu_callback *);
};

void rcu_tick(void);
void rcu_report_qs(void);

void rcu_read_lock(void);
void rcu_read_unlock(void);
void rcu_call(struct rcu_callback *cb, void (*fptr)(struct rcu_callback *));

void rcu_setup(void);
void rcu_cpu_setup(void);

#endif /*__RCU_H__*/
