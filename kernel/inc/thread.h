#ifndef __THREAD_H__
#define __THREAD_H__

#include <stdint.h>

struct thread {
	uintptr_t stack_ptr;
	uintptr_t stack_addr;
	int stack_order;
};

typedef void (*thread_fptr_t)(void *);

void threads_setup(void);
void threads_cpu_setup(void);

struct thread *__thread_create(thread_fptr_t fptr, void *arg, int stack_order);
struct thread *thread_create(thread_fptr_t fptr, void *arg);
void thread_destroy(struct thread *thread);

struct thread *thread_current(void);
void thread_switch_to(struct thread *target);

#endif /*__THREAD_H__*/
