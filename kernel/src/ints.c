#include <backtrace.h>
#include <memory.h>
#include <thread.h>
#include <stdint.h>
#include <debug.h>
#include <apic.h>
#include <ints.h>
#include <cpu.h>


#define IDT_DPL(x)	(((unsigned)(x) & 0x3u) << 13)
#define IDT_USER_MODE	IDT_DPL(3u)
#define IDT_KERNEL_MODE	IDT_DPL(0u)

#define IDT_TYPE(x)	(((unsigned)(x) & 0xfu) << 8)
#define IDT_INT_GATE	IDT_TYPE(0xeu)
#define IDT_TRAP_GATE	IDT_TYPE(0xfu)

#define IDT_PRESENT	(1u << 15)
#define IDT_EXCEPTION	(IDT_KERNEL_MODE | IDT_INT_GATE | IDT_PRESENT)
#define IDT_IRQ		(IDT_KERNEL_MODE | IDT_INT_GATE | IDT_PRESENT)

struct idt_desc {
	uint16_t offs0;
	uint16_t sel;
	uint16_t flags;
	uint16_t offs1;
	uint32_t offs2;
	uint32_t reserved;
} __attribute__((packed));


static struct idt_desc idt[IDT_SIZE] __attribute__((aligned (16)));
static struct irq_desc irq_desc[IDT_IRQ_END - IDT_IRQ_BEGIN];
static exception_handler_t exc_handler[IDT_EXC_END - IDT_EXC_BEGIN];

typedef void(*int_raw_entry_t)(void);
extern int_raw_entry_t int_raw_handler_table[];


static void exception_handle(struct frame *frame)
{
	if (exc_handler[frame->num]) {
		exc_handler[frame->num](frame);
		return;
	}

	struct thread *thread = thread_current();

	if (thread) {
		const int order = thread->stack_order + PAGE_SHIFT;
		const uintptr_t size = 1ul << order;
		const uintptr_t begin = thread->stack_addr;
		const uintptr_t end = begin + size;

		printf("Backtrace Begin:\n");
		__backtrace(frame->rbp, begin, end);
		printf("Backtrace End.\n");
	}
	printf("Register State:\n");
	printf("RIP 0x%lx, RSP 0x%lx, RBP 0x%lx,\n"
		"R8 0x%lx, R9 0x%lx, R10 0x%lx, R11 0x%lx,\n"
		"R12 0x%lx, R13 0x%lx, R14 0x%lx, R15 0x%lx,\n"
		"RAX 0x%lx, RBX 0x%lx, RCX 0x%lx, RDX 0x%lx,\n"
		"RSI 0x%lx, RDI 0x%lx\n", frame->rip, frame->rsp, frame->rbp,
		frame->r8, frame->r9, frame->r10, frame->r11,
		frame->r12, frame->r13, frame->r14, frame->r15,
		frame->rax, frame->rbx, frame->rcx, frame->rdx,
		frame->rsi, frame->rdi);
	BUG("Unhandled exception %d err %lu, at 0x%lx",
				(int)frame->num, (unsigned long)frame->err,
				(unsigned long)frame->rip);
}

static void ack_irq(int irq)
{
	/* TODO: suppress broadcast EOI and for level triggered interrupts
	 * ack corresponding io_apic explicitly. */
	(void) irq;
	local_apic_write(0xb0, 0);
}

static void irq_handle(int irq)
{
	BUG_ON(irq >= IDT_IRQ_END - IDT_IRQ_BEGIN);

	const struct irq_desc *desc = &irq_desc[irq];

	BUG_ON(!desc->handler);
	ack_irq(irq);
	desc->handler();
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
	BUG_ON(irq >= IDT_IRQ_END - IDT_IRQ_BEGIN);
	struct irq_desc *desc = &irq_desc[irq];

	BUG_ON(desc->handler);
	desc->handler = handler;
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

static void idt_setup(void)
{
	const struct desc_ptr ptr = {
		.limit = sizeof(idt) - 1,
		.base = (uint64_t)idt
	};

	write_idt(&ptr);
}

static void local_apic_setup(void)
{
	const unsigned long spurious = local_apic_read(APIC_SPURIOUS);

	local_apic_write(APIC_SPURIOUS, spurious | APIC_ENABLE);
}

void ints_early_setup(void)
{
	for (int i = IDT_EXC_BEGIN; i != IDT_EXC_END; ++i) {
		const uintptr_t handler = (uintptr_t)int_raw_handler_table[i];

		idt_desc_set(&idt[i], KERNEL_CS, handler, IDT_EXCEPTION);
	}

	for (int i = IDT_IRQ_BEGIN; i != IDT_IRQ_END; ++i) {
		const uintptr_t handler = (uintptr_t)int_raw_handler_table[i];

		idt_desc_set(&idt[i], KERNEL_CS, handler, IDT_IRQ);
	}
	idt_setup();
}

void ints_cpu_setup(void)
{
	idt_setup();
	local_apic_setup();
}

void activate_irq(int irq)
{
	struct irq_desc *desc = &irq_desc[irq];

	BUG_ON(!desc->handler);
	BUG_ON(!desc->ops || !desc->ops->irq_unmask);
	desc->ops->irq_unmask(desc);
}

void deactivate_irq(int irq)
{
	struct irq_desc *desc = &irq_desc[irq];

	BUG_ON(!desc->ops || !desc->ops->irq_mask);
	desc->ops->irq_mask(desc);
}


static void io_apic_mask(struct irq_desc *desc)
{
	struct io_apic *apic = desc->apic;
	const int pin = desc->pin;

	BUG_ON(!apic);
	io_apic_write(apic, IO_APIC_RDREG_LOW(pin), IO_APIC_MASK_PIN);
}

static void io_apic_unmask(struct irq_desc *desc)
{
	struct io_apic *apic = desc->apic;
	const int pin = desc->pin;
	unsigned long low;

	BUG_ON(!apic);
	low = io_apic_read(apic, IO_APIC_RDREG_LOW(pin));
	io_apic_write(apic, IO_APIC_RDREG_LOW(pin), low & ~IO_APIC_MASK_PIN);
}

static const struct irq_desc_ops io_apic_ops = {
	.irq_mask = io_apic_mask,
	.irq_unmask = io_apic_unmask
};

static void io_apic_setup_pin(struct io_apic *apic, int pin, int vector,
			const struct irq_info *info)
{
	const unsigned long low = IO_APIC_VECTOR(vector) | IO_APIC_LOWEST
				| IO_APIC_LOGICAL | IO_APIC_MASK_PIN
				| (info->trigger == INT_EDGE ?
				  IO_APIC_EDGE : IO_APIC_LEVEL)
				| (info->polarity == INT_ACTIVE_LOW ?
				  IO_APIC_ACTIVE_LOW : IO_APIC_ACTIVE_HIGH);
	const unsigned long high = IO_APIC_DESTINATION(0xff);

	io_apic_write(apic, IO_APIC_RDREG_LOW(pin), low);
	io_apic_write(apic, IO_APIC_RDREG_HIGH(pin), high);
}

void register_irq(int irq, const struct irq_info *info)
{
	BUG_ON(irq >= IDT_IRQ_END - IDT_IRQ_BEGIN);
	struct irq_desc *desc = &irq_desc[irq];
	const int vector = IRQ_VECTOR(irq);
	struct io_apic *apic = info->apic;

	if (apic)
		io_apic_setup_pin(apic, info->pin, vector, info);

	desc->ops = (apic && !info->ops) ? &io_apic_ops : info->ops;
	desc->apic = info->apic;
	desc->pin = info->pin;
	desc->trigger = info->trigger;
	desc->polarity = info->polarity;
}
