#include <backtrace.h>
#include <memory.h>
#include <thread.h>
#include <print.h>

#define RBP(x)	__asm__ ("movq %%rbp, %0" : "=rm"(x))

void __backtrace(uintptr_t rbp, uintptr_t stack_begin, uintptr_t stack_end)
{
	int frame_index = 0;

	while (rbp >= stack_begin && rbp + 16 <= stack_end) {
		const uint64_t *frame = (const uint64_t *)rbp;
		const uintptr_t prev_rbp = frame[0];
		const uintptr_t prev_rip = frame[1];

		if (prev_rbp <= rbp)
			break;

		printf("%d: rip 0x%lx\n", frame_index, (unsigned long)prev_rip);
		rbp = prev_rbp;
		++frame_index;
	}
}

void backtrace(void)
{
	struct thread *thread = thread_current();

	if (!thread)
		return;

	const int order = thread->stack_order + PAGE_SHIFT;
	const uintptr_t size = 1ul << order;
	const uintptr_t begin = thread->stack_addr;
	const uintptr_t end = begin + size;

	uintptr_t rbp;
	RBP(rbp);

	__backtrace(rbp, begin, end);
}
