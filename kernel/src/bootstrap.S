#define MB_HEADER_MAGIC 0x1BADB002
#define MB_HEADER_FLAGS ((1 << 16) | (1 << 1))
#define MB_HEADER_CKSUM -(MB_HEADER_MAGIC + MB_HEADER_FLAGS)

	.section .bootstrap, "ax"
	.code32
	.global start32
	.extern main

	.align 16
start32:
	jmp 1f

	.align 16
multiboot_header:
	.long MB_HEADER_MAGIC
	.long MB_HEADER_FLAGS
	.long MB_HEADER_CKSUM
	.long multiboot_header
	.long text_phys_begin
	.long data_phys_end
	.long bss_phys_end
	.long start32

	.align 16
	.global tss
tss:
tss_begin:
	.space 105
tss_end:

	.align 16
gdt:
        .quad 0x0000000000000000
	.quad 0x00209b0000000000
	.quad 0x0020930000000000
	.quad 0x0000000000000000
	.quad 0x0000000000000000
gdt_ptr:
	.word (gdt_ptr - gdt - 1)
	.long gdt

1:
	/* Save pointer to multiboot info structure */
	movl %ebx, %esi

	/* Create identity mapping for lower 512GB using 1GB pages:
	 *  - pml3 table contains 512 entries by 8 bytes each
	 *  - pml3 entry points to 1GB physical page
	 *  - pml4 table contains 512 entries by 8 bytes each
	 *  - pml4 entry points to pml3 table */
#define PTE_PRESENT (1 << 0)
#define PTE_WRITE   (1 << 1)
#define PTE_LARGE   (1 << 7)
	movl $bootstrap_pml3, %edi
	movl $(PTE_PRESENT | PTE_WRITE | PTE_LARGE), %eax
	movl $0, %ebx
	movl $512, %ecx

1:
	movl %eax, (%edi)
	movl %ebx, 4(%edi)
	add $0x40000000, %eax
	adc $0, %ebx
	add $8, %edi
	loop 1b

	movl $bootstrap_pml4, %edi
	movl $bootstrap_pml3, %eax
	orl $(PTE_PRESENT | PTE_WRITE), %eax
	movl %eax, (%edi)

	/* We need to enable PAE bit according to Intel's manual */
#define CR4_PAE     (1 << 5)
	movl %cr4, %eax
	orl $CR4_PAE, %eax
	movl %eax, %cr4

	/* Set pointer to pml4 table in cr3 register */
	movl $bootstrap_pml4, %eax
	movl %eax, %cr3
	
	/* Enable long mode */
#define EFER_LM     (1 << 8)
	movl $0xC0000080, %ecx
	rdmsr
	orl $EFER_LM, %eax
	wrmsr

	/* Enable paging */
#define CR0_PG      (1 << 31)
#define CR0_NE      (1 << 5)
	movl %cr0, %eax
	orl $(CR0_PG | CR0_NE), %eax
	movl %eax, %cr0

	/* Enable 64bit gdt */
	lgdt gdt_ptr
	ljmpl $0x08, $start64

	.code64
start64:
	cli
	movw $0x10, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs
	movw %ax, %ss

	movabsq $bootstrap_stack_top, %rax
	movq %rax, %rsp

	xorq %rdi, %rdi
	movl %esi, %edi

	cld
	movabsq $main, %rax
	call *%rax

	cli
1:
	hlt
	jmp 1b

	.bss
	.align 0x1000
	.space 0x1000
bootstrap_stack_top:
bootstrap_pml4:
	.space 0x1000
bootstrap_pml3:
	.space 0x1000
