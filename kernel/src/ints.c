#include <stdint.h>
#include <debug.h>
#include <ints.h>


#define IDT_DPL(x)	(((unsigned)(x) & 0x3u) << 13)
#define IDT_USER_MODE	IDT_DPL(3u)
#define IDT_KERNEL_MODE	IDT_DPL(0u)

#define IDT_TYPE(x)	(((unsigned)(x) & 0xfu) << 8)
#define IDT_INT_GATE	IDT_TYPE(0xeu)
#define IDT_TRAP_GATE	IDT_TYPE(0xfu)

#define IDT_PRESENT	(1u << 15)
#define IDT_EXCEPTION	(IDT_KERNEL_MODE | IDT_INT_GATE | IDT_PRESENT)
#define IDT_IRQ		(IDT_KERNEL_MODE | IDT_INT_GATE | IDT_PRESENT)

#define IDT_SIZE	33
#define IDT_EXC_BEGIN	0
#define IDT_EXC_END	32
#define IDT_IRQ_BEGIN	32
#define IDT_IRQ_END	IDT_SIZE

struct idt_ptr {
	uint16_t size;
	uint64_t idt;
} __attribute__((packed));

struct idt_desc {
	uint16_t offs0;
	uint16_t sel;
	uint16_t flags;
	uint16_t offs1;
	uint32_t offs2;
	uint32_t reserved;
} __attribute__((packed));

static struct idt_desc idt[IDT_SIZE] __attribute__((aligned (16)));
static irq_handler_t irq_handler[IDT_IRQ_END - IDT_IRQ_BEGIN];
static exception_handler_t exc_handler[IDT_EXC_END - IDT_EXC_BEGIN];

typedef void(*int_raw_entry_t)(void);
extern int_raw_entry_t int_raw_handler_table[];


static void exception_handle(struct frame *frame)
{
	if (exc_handler[frame->num]) {
		exc_handler[frame->num](frame);
		return;
	}
	BUG("Unhandled exception %d err %lu",
				(int)frame->num, (unsigned long)frame->err);
}

static void irq_handle(int irq)
{
	BUG_ON(irq >= IDT_IRQ_END - IDT_IRQ_BEGIN);
	BUG_ON(!irq_handler[irq]);
	irq_handler[irq]();
}

void isr_handler(struct frame *frame)
{
	const int num = frame->num;

	if (num >= IDT_IRQ_BEGIN) {
		irq_handle(num - IDT_IRQ_BEGIN);
		return;
	}
	exception_handle(frame);
}

void register_exception_handler(int exception, exception_handler_t handler)
{
	BUG_ON(exc_handler[exception]);
	exc_handler[exception] = handler;
}

void register_irq_handler(int irq, irq_handler_t handler)
{
	BUG_ON(irq_handler[irq]);
	irq_handler[irq] = handler;
}

static void idt_desc_set(struct idt_desc *desc, unsigned sel, uintptr_t offs,
			unsigned flags)
{
	desc->offs0 = offs & 0xffffu;
	desc->offs1 = (offs >> 16) & 0xffffu;
	desc->offs2 = (offs >> 32) & 0xfffffffful;

	desc->sel = sel;
	desc->flags = flags;
	desc->reserved = 0;
}

void ints_setup(void)
{
	for (int i = IDT_EXC_BEGIN; i != IDT_EXC_END; ++i) {
		const uintptr_t handler = (uintptr_t)int_raw_handler_table[i];

		idt_desc_set(&idt[i], 0x08, handler, IDT_EXCEPTION);
	}

	for (int i = IDT_IRQ_BEGIN; i != IDT_IRQ_END; ++i) {
		const uintptr_t handler = (uintptr_t)int_raw_handler_table[i];

		idt_desc_set(&idt[i], 0x08, handler, IDT_IRQ);
	}
}

void cpu_ints_setup(void)
{
	const struct idt_ptr ptr = {
		.size = sizeof(idt) - 1,
		.idt = (uint64_t)idt
	};

	__asm__ volatile ("lidt %0" : : "m"(ptr)); 
}
