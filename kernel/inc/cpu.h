#ifndef __CPU_H__
#define __CPU_H__

#include <stdint.h>

#define CR4_VMXE	(1ul << 13)
#define RFLAGS_CF	(1ul << 0)
#define RFLAGS_ZF	(1ul << 6)
#define KERNEL_TSS	0x18
#define KERNEL_DATA	0x10
#define KERNEL_CODE	0x08

struct desc_ptr {
	uint16_t limit;
	uint64_t base;
} __attribute__((packed));

static inline void cpu_relax(void)
{ __asm__ volatile("pause"); }

static inline void cpuid(unsigned long *eax, unsigned long *ebx,
			unsigned long *ecx, unsigned long *edx)
{
	__asm__("cpuid"
		: "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
		: "a"(*eax));
}

static inline unsigned long long read_msr(unsigned long msr)
{
	unsigned long low, high;

	__asm__ ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
	return ((unsigned long long)(high & 0xfffffffful) << 32)
			| ((unsigned long long)low & 0xfffffffful);
}

static inline void write_msr(unsigned long msr, unsigned long long val)
{
	const unsigned long low = val & 0xfffffffful;
	const unsigned long high = (val >> 32) & 0xfffffffful;

	__asm__ ("wrmsr" : : "a"(low), "d"(high), "c"(msr));
}

static inline unsigned long long read_cr0(void)
{
	unsigned long long cr0;

	__asm__ ("movq %%cr0, %0" : "=a"(cr0));
	return cr0;
}

static inline void write_cr0(unsigned long long cr0)
{
	__asm__ ("movq %0, %%cr0" : : "ad"(cr0));
}

static inline unsigned long long read_cr3(void)
{
	unsigned long long cr3;

	__asm__ ("movq %%cr3, %0" : "=a"(cr3));
	return cr3;
}

static inline void write_cr3(unsigned long long cr3)
{
	__asm__ ("movq %0, %%cr3" : : "ad"(cr3));
}

static inline unsigned long long read_cr4(void)
{
	unsigned long long cr4;

	__asm__ ("movq %%cr4, %0" : "=a"(cr4));
	return cr4;
}

static inline void write_cr4(unsigned long long cr4)
{
	__asm__ ("movq %0, %%cr4" : : "a"(cr4));
}

static inline void read_gdt(struct desc_ptr *ptr)
{
	__asm__ ("sgdt %0" : "=m"(*ptr));
}

static inline void read_idt(struct desc_ptr *ptr)
{
	__asm__ ("sidt %0" : "=m"(*ptr));
}

static inline void write_tr(unsigned short sel)
{
	__asm__ ("ltr %0" : : "r"(sel));
}

static inline unsigned short read_tr(void)
{
	unsigned short tr;

	__asm__ ("str %0" : "=a"(tr));
	return tr;
}

void tss_setup(void);
void tss_cpu_setup(void);

#endif /*__CPU_H__*/
