	.text
	.code64

#define FOR_EACH_VECTOR(ISR, EXC) \
	ISR(0) \
	ISR(1) \
	ISR(2) \
	ISR(3) \
	ISR(4) \
	ISR(5) \
	ISR(6) \
	ISR(7) \
	EXC(8) \
	ISR(9) \
	EXC(10) \
	EXC(11) \
	EXC(12) \
	EXC(13) \
	EXC(14) \
	ISR(15) \
	ISR(16) \
	EXC(17) \
	ISR(18) \
	ISR(19) \
	ISR(20) \
	ISR(21) \
	ISR(22) \
	ISR(23) \
	ISR(24) \
	ISR(25) \
	ISR(26) \
	ISR(27) \
	ISR(28) \
	ISR(29) \
	ISR(30) \
	ISR(31) \
	ISR(32)

#define NAME(num) entry ## num

	.global int_raw_handler_table
	.align 32
#define VECTOR(num) .quad NAME(num);
int_raw_handler_table:
	FOR_EACH_VECTOR(VECTOR, VECTOR)
#undef VECTOR

	.extern isr_handler
__entry:
	subq $72, %rsp
	movq %r11, 0(%rsp)
	movq %r10, 8(%rsp)
	movq %r9, 16(%rsp)
	movq %r8, 24(%rsp)
	movq %rax, 32(%rsp)
	movq %rcx, 40(%rsp)
	movq %rdx, 48(%rsp)
	movq %rsi, 56(%rsp)
	movq %rdi, 64(%rsp)

	cld
	movq %rsp, %rdi
	call isr_handler

	movq 0(%rsp), %r11
	movq 8(%rsp), %r10
	movq 16(%rsp), %r9
	movq 24(%rsp), %r8
	movq 32(%rsp), %rax
	movq 40(%rsp), %rcx
	movq 48(%rsp), %rdx
	movq 56(%rsp), %rsi
	movq 64(%rsp), %rdi
	addq $88, %rsp
	iretq

#define EXC(num) \
	.align 16; \
NAME(num): \
	pushq $num; \
	jmp __entry;

#define ISR(num) \
	.align 16; \
NAME(num): \
	pushq $0; \
	pushq $num; \
	jmp __entry;

FOR_EACH_VECTOR(ISR, EXC)

#undef ISR
#undef EXC

