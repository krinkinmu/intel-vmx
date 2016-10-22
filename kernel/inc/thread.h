#ifndef __THREAD_H__
#define __THREAD_H__

#include <stdatomic.h>
#include <stdint.h>
#include <list.h>

enum thread_state {
	THREAD_ACTIVE,
	THREAD_BLOCKED,
	THREAD_FINISHING,
	THREAD_FINISHED
};

struct thread {
	struct list_head ll;
	unsigned long long timestamp;
	uintptr_t stack_ptr;
	uintptr_t stack_addr;
	void *fpu_state;
	int stack_order;
	atomic_int state;
};

typedef void (*thread_fptr_t)(void *);

void threads_setup(void);
void threads_cpu_setup(void);

struct thread *__thread_create(thread_fptr_t fptr, void *arg, int stack_order);
struct thread *thread_create(thread_fptr_t fptr, void *arg);
void thread_join(struct thread *thread);
void thread_destroy(struct thread *thread);

void thread_activate(struct thread *thread);
void thread_set_state(struct thread *thread, int state);
int thread_get_state(struct thread *thread);

struct thread *thread_current(void);
void thread_switch_to(struct thread *target);

#endif /*__THREAD_H__*/
