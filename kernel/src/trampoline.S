	.code16
	.global tr_begin, tr_end
	.extern bootstrap_gdt
	.align 0x1000
tr_base = .
tr_begin:
	cli
	movw %cs, %ax
	movw %ax, %ds
	movw %ax, %es

	lidt bootstrap_idt_ptr - tr_base
	lgdt bootstrap_gdt_ptr - tr_base

#define CR4_PAE (1 << 5)
	movl %cr4, %eax
	orl $CR4_PAE, %eax
	movl %eax, %cr4

	movl pgtable - tr_base, %eax
	movl %eax, %cr3

#define EFER_LM (1 << 8)
	movl $0xC0000080, %ecx
	rdmsr
	orl $EFER_LM, %eax
	wrmsr

#define CR0_PG (1 << 31)
#define CR0_PM (1 << 0)
#define CR0_NE (1 << 5)
	movl %cr0, %eax
	orl $(CR0_PG | CR0_PM | CR0_NE), %eax
	movl %eax, %cr0

	movl stackend - tr_base, %eax
	movl %eax, %esp

	movl startup - tr_base, %ebx
	ljmpl $0x08, $ap_start64

bootstrap_gdt_ptr:
	.word 0xffff
	.long bootstrap_gdt
bootstrap_idt_ptr:
	.word 0
	.long 0

	.align 4
	.long 0x13131313
pgtable:
	.long 0x0
stackend:
	.long 0x0
startup:
	.long 0x0
tr_end:

	.code64
ap_start64:
	movw $0x10, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs
	movw %ax, %ss

	xorq %rax, %rax
	movl %ebx, %eax
	call *%rax

	cli
1:
	hlt
	jmp 1b
