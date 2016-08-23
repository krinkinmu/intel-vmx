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
	movl %cr0, %eax
	orl $(CR0_PG | CR0_PM), %eax
	movl %eax, %cr0

	movl stackend - tr_base, %eax
	movl %eax, %esp

	ljmpl *(startup - tr_base)
1:	jmp 1b

	.long 0x13131313
pgtable:
	.long 0x0
stackend:
	.long 0x0
startup:
	.long 0x0
	.word 0x0
tr_gdt:
	.word 0x0
	.long 0x0
tr_idt:
	.word 0x0
	.long 0x0
tr_end:
