#ifndef __CPU_H__
#define __CPU_H__

#define CR4_VMXE	(1ul << 13)
#define RFLAGS_CF	(1ul << 0)
#define RFLAGS_ZF	(1ul << 6)

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

#endif /*__CPU_H__*/
