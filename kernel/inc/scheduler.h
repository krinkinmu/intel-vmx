#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

struct thread;

struct thread *scheduler_next_thread(void);
void scheduler_activate_thread(struct thread *thread);
void scheduler_block_thread(void);

void scheduler_setup(void);
void scheduler_cpu_setup(void);

#endif /*__SCHEDULER_H__*/
