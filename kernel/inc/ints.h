#ifndef __INTERRUPT_H__
#define __INTERRUPT_H__

#include <stdint.h>

struct frame {
	uint64_t r11;
	uint64_t r10;
	uint64_t r9;
	uint64_t r8;
	uint64_t rax;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t num;
	uint64_t err;
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
} __attribute__((packed));

typedef void(*irq_handler_t)(void);
typedef void(*exception_handler_t)(struct frame *frame);

void register_exception_handler(int exception, exception_handler_t handler);
void register_irq_handler(int irq, irq_handler_t handler);

static inline void local_int_enable(void)
{ __asm__ volatile ("sti"); }

static inline void local_int_disable(void)
{ __asm__ volatile ("cli"); }

void ints_setup(void);
void cpu_ints_setup(void);

#endif /*__INTRRUPT_H__*/
