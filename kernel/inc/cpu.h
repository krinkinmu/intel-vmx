#ifndef __CPU_H__
#define __CPU_H__

#include <stdint.h>

#define CR4_OSFXSR	(1ul << 9)
#define CR4_OSXMMEXCPT	(1ul << 10)
#define CR4_VMXE	(1ul << 13)
#define CR4_OSXSAVE	(1ul << 18)
#define CR0_MP		(1ul << 1)
#define CR0_EM		(1ul << 2)
#define CR0_TS		(1ul << 3)
#define RFLAGS_CF	(1ul << 0)
#define RFLAGS_ZF	(1ul << 6)
#define RFLAGS_IF	(1ul << 9)
#define RFLAGS_RESERVED	(1ul << 2)
#define KERNEL_TSS	0x18
#define KERNEL_DATA	0x10
#define KERNEL_CODE	0x08

#define MAX_CPU_NR	8

struct desc_ptr {
	uint16_t limit;
	uint64_t base;
} __attribute__((packed));

static inline void cpu_relax(void)
{ __asm__ volatile("pause"); }

static inline void cpuid(unsigned long *eax, unsigned long *ebx,
			unsigned long *ecx, unsigned long *edx)
{
	__asm__ volatile ("cpuid"
		: "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
		: "a"(*eax));
}

static inline unsigned long long read_msr(unsigned long msr)
{
	unsigned long low, high;

	__asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
	return ((unsigned long long)(high & 0xfffffffful) << 32)
			| ((unsigned long long)low & 0xfffffffful);
}

static inline void write_msr(unsigned long msr, unsigned long long val)
{
	const unsigned long low = val & 0xfffffffful;
	const unsigned long high = (val >> 32) & 0xfffffffful;

	__asm__ volatile ("wrmsr" : : "a"(low), "d"(high), "c"(msr));
}

static inline unsigned long long read_cr0(void)
{
	unsigned long long cr0;

	__asm__ volatile ("movq %%cr0, %0" : "=a"(cr0));
	return cr0;
}

static inline void write_cr0(unsigned long long cr0)
{
	__asm__ volatile ("movq %0, %%cr0" : : "ad"(cr0));
}

static inline unsigned long long read_cr3(void)
{
	unsigned long long cr3;

	__asm__ volatile ("movq %%cr3, %0" : "=a"(cr3));
	return cr3;
}

static inline void write_cr3(unsigned long long cr3)
{
	__asm__ volatile ("movq %0, %%cr3" : : "ad"(cr3) : "memory");
}

static inline unsigned long long read_cr4(void)
{
	unsigned long long cr4;

	__asm__ volatile ("movq %%cr4, %0" : "=a"(cr4));
	return cr4;
}

static inline void write_cr4(unsigned long long cr4)
{
	__asm__ volatile ("movq %0, %%cr4" : : "a"(cr4));
}

static inline void write_gdt(const struct desc_ptr *ptr)
{
	__asm__ volatile ("lgdt %0" : : "m"(*ptr));
}

static inline void read_gdt(struct desc_ptr *ptr)
{
	__asm__ volatile ("sgdt %0" : "=m"(*ptr));
}

static inline void write_idt(const struct desc_ptr *ptr)
{
	__asm__ volatile ("lidt %0" : : "m"(*ptr));
}

static inline void read_idt(struct desc_ptr *ptr)
{
	__asm__ volatile ("sidt %0" : "=m"(*ptr));
}

static inline void write_tr(unsigned short sel)
{
	__asm__ volatile ("ltr %0" : : "r"(sel));
}

static inline unsigned short read_tr(void)
{
	unsigned short tr;

	__asm__ volatile ("str %0" : "=a"(tr));
	return tr;
}

static inline unsigned long rflags(void)
{
	unsigned long flags;

	__asm__ volatile ("pushf ; pop %0" : "=a"(flags) : : "memory");

	return flags;
}

void cpu_setup(void);
int cpu_id(void);
int cpu_count(void);

#endif /*__CPU_H__*/
