	.text
	.code64
	.global __thread_switch, __thread_entry
	.extern thread_entry

__thread_entry:
	movq %r12, %rdi
	movq %r13, %rsi
	call thread_entry
1:	jmp 1b

__thread_switch:
	pushfq
	pushq %rbp
	pushq %rbx
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15

	movq %rsp, (%rdi)
	movq %rsi, %rsp

	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbx
	popq %rbp
	popfq

	ret

#define FOR_EACH_VECTOR(NOERR, ERR) \
	NOERR(0) \
	NOERR(1) \
	NOERR(2) \
	NOERR(3) \
	NOERR(4) \
	NOERR(5) \
	NOERR(6) \
	NOERR(7) \
	ERR(8) \
	NOERR(9) \
	ERR(10) \
	ERR(11) \
	ERR(12) \
	ERR(13) \
	ERR(14) \
	NOERR(15) \
	NOERR(16) \
	ERR(17) \
	NOERR(18) \
	NOERR(19) \
	NOERR(20) \
	NOERR(21) \
	NOERR(22) \
	NOERR(23) \
	NOERR(24) \
	NOERR(25) \
	NOERR(26) \
	NOERR(27) \
	NOERR(28) \
	NOERR(29) \
	NOERR(30) \
	NOERR(31) \
	NOERR(32) \
	NOERR(33)

#define NAME(num) entry ## num

	.global int_raw_handler_table
	.align 32
#define VECTOR(num) .quad NAME(num);
int_raw_handler_table:
	FOR_EACH_VECTOR(VECTOR, VECTOR)
#undef VECTOR

	.extern isr_handler
__entry:
	subq $120, %rsp
	movq %rbp, 0(%rsp)
	movq %rbx, 8(%rsp)
	movq %r15, 16(%rsp)
	movq %r14, 24(%rsp)
	movq %r13, 32(%rsp)
	movq %r12, 40(%rsp)
	movq %r11, 48(%rsp)
	movq %r10, 56(%rsp)
	movq %r9, 64(%rsp)
	movq %r8, 72(%rsp)
	movq %rax, 80(%rsp)
	movq %rcx, 88(%rsp)
	movq %rdx, 96(%rsp)
	movq %rsi, 104(%rsp)
	movq %rdi, 112(%rsp)

	cld
	movq %rsp, %rdi
	call isr_handler

	movq 0(%rsp), %rbp
	movq 8(%rsp), %rbx
	movq 16(%rsp), %r15
	movq 24(%rsp), %r14
	movq 32(%rsp), %r13
	movq 40(%rsp), %r12
	movq 48(%rsp), %r11
	movq 56(%rsp), %r10
	movq 64(%rsp), %r9
	movq 72(%rsp), %r8
	movq 80(%rsp), %rax
	movq 88(%rsp), %rcx
	movq 96(%rsp), %rdx
	movq 104(%rsp), %rsi
	movq 112(%rsp), %rdi
	addq $136, %rsp
	iretq

#define ERR(num) \
	.align 16; \
NAME(num): \
	pushq $num; \
	jmp __entry;

#define NOERR(num) \
	.align 16; \
NAME(num): \
	pushq $0; \
	pushq $num; \
	jmp __entry;

FOR_EACH_VECTOR(NOERR, ERR)

#undef ERR
#undef NOERR

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
