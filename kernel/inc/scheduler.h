#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

struct thread;

void scheduler_activate_thread(struct thread *thread);

void preempt_disable(void);
void preempt_enable(void);
void schedule(void);

void scheduler_setup(void);
void scheduler_cpu_setup(void);

#endif /*__SCHEDULER_H__*/
