	.data
	.code16
	.global tr_begin, tr_end
	.align 0x1000
tr_base = .
tr_begin:
	cli
	movw %cs, %ax
	movw %ax, %ds
	movw %ax, %es

	lidt tr_idt - tr_base
	lgdt tr_gdt - tr_base

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

	movl startup - tr_base, %eax
	pushl $0x0
	pushl %eax
	ljmpl $0x08, $start64

	.align 4
	.long 0x13131313
pgtable:
	.long 0x0
stackend:
	.long 0x0
startup:
	.long 0x0
tr_gdt:
	.word 0x0
	.long 0x0
tr_idt:
	.word 0x0
	.long 0x0
tr_end:

	.code64
start64:
	movw $0x10, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs
	movw %ax, %ss

	movw $0x18, %ax
	ltr %ax

	popq %rax
	call *%rax

	cli
1:
	hlt
	jmp 1b
