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
	ISR(32) \
	ISR(33)

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

vmcs_return:
	pushq %rax
	movq 8(%rsp), %rax
	movq %rbx, 0x08(%rax)
	movq %rcx, 0x10(%rax)
	movq %rdx, 0x18(%rax)
	movq %rbp, 0x20(%rax)
	movq %rsi, 0x28(%rax)
	movq %rdi, 0x30(%rax)
	movq %r8, 0x38(%rax)
	movq %r9, 0x40(%rax)
	movq %r10, 0x48(%rax)
	movq %r11, 0x50(%rax)
	movq %r12, 0x58(%rax)
	movq %r13, 0x60(%rax)
	movq %r14, 0x68(%rax)
	movq %r15, 0x70(%rax)
	popq %rax
	popq %rdi
	movq %rax, 0x00(%rdi)

	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbp
	popq %rbx

	movq $0, %rax
	retq

vmcs_error:
	popq %rdi
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbp
	popq %rbx

	movq $-1, %rax
	retq

#define VMCS_HOST_RSP	0x6c14
#define VMCS_HOST_RIP	0x6c16

	.global __vmcs_launch
__vmcs_launch:
	pushq %rbx
	pushq %rbp
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
	pushq %rdi

	movq $vmcs_return, %rax
	movq $VMCS_HOST_RIP, %rbx
	vmwrite %rax, %rbx
	jna vmcs_error

	movq %rsp, %rax
	movq $VMCS_HOST_RSP, %rbx
	vmwrite %rax, %rbx
	jna vmcs_error

	movq 0x00(%rdi), %rax
	movq 0x08(%rdi), %rbx
	movq 0x10(%rdi), %rcx
	movq 0x18(%rdi), %rdx
	movq 0x20(%rdi), %rbp
	movq 0x28(%rdi), %rsi
	movq 0x38(%rdi), %r8
	movq 0x40(%rdi), %r9
	movq 0x48(%rdi), %r10
	movq 0x50(%rdi), %r11
	movq 0x58(%rdi), %r12
	movq 0x60(%rdi), %r13
	movq 0x68(%rdi), %r14
	movq 0x70(%rdi), %r15
	movq 0x30(%rdi), %rdi

	vmlaunch
	jmp vmcs_error

	.global __vmcs_resume
__vmcs_resume:
	pushq %rbx
	pushq %rbp
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
	pushq %rdi

	movq $vmcs_return, %rax
	movq $VMCS_HOST_RIP, %rbx
	vmwrite %rax, %rbx
	jna vmcs_error

	movq %rsp, %rax
	movq $VMCS_HOST_RSP, %rbx
	vmwrite %rax, %rbx
	jna vmcs_error

	movq 0x00(%rdi), %rax
	movq 0x08(%rdi), %rbx
	movq 0x10(%rdi), %rcx
	movq 0x18(%rdi), %rdx
	movq 0x20(%rdi), %rbp
	movq 0x28(%rdi), %rsi
	movq 0x38(%rdi), %r8
	movq 0x40(%rdi), %r9
	movq 0x48(%rdi), %r10
	movq 0x50(%rdi), %r11
	movq 0x58(%rdi), %r12
	movq 0x60(%rdi), %r13
	movq 0x68(%rdi), %r14
	movq 0x70(%rdi), %r15
	movq 0x30(%rdi), %rdi

	vmresume
	jmp vmcs_error
