#include <fpu.h>
#include <cpu.h>
#include <debug.h>
#include <string.h>

#include <stdint.h>
#include <limits.h>

#define CPUID_XSAVE	(1ul << 26)
#define CPUID_XSAVEOPT	(1ul << 0)

#define X87_FPU_MASK	(1ull << 0)
#define SSE_MASK	(1ull << 1)
#define AVX_MASK	(1ull << 2)
#define EXT_MASK	(X87_FPU_MASK | SSE_MASK | AVX_MASK)
#define XSAVE_MASK(x)	(((unsigned long long)(x) & EXT_MASK) \
				| (X87_FPU_MASK | SSE_MASK))

static unsigned long long xcr0_features;
static int state_size;

int fpu_state_size(void)
{
	return state_size;
}

void fpu_state_setup(void *state)
{
	BUG_ON((uintptr_t)state & 0x3f);
	memset(state, 0, state_size);
}

void fpu_state_save(void *state)
{
	//const unsigned long edx = xcr0_features >> 32;
	//const unsigned long eax = xcr0_features & 0xfffffffful;

	// causes GP for unknown reason in qemu, according to
	// docs GP may be generated only because state is not
	// 64 byte aligned, but it's not the case, so it must
	// be some other error.

	(void) state;
 
	//__asm__ volatile ("xsave %0"
	//			:
	//			: "m"(state), "d"(edx), "a"(eax)
	//			: "memory");
}

void fpu_state_restore(const void *state)
{
	//const unsigned long edx = xcr0_features >> 32;
	//const unsigned long eax = xcr0_features & 0xfffffffful;

	(void) state;

	//__asm__ volatile ("xrstor %0"
	//			:
	//			: "m"(state), "d"(edx), "a"(eax)
	//			: "memory");
}

static void __xcr0_write(unsigned long long features)
{
	const unsigned long edx = features >> 32;
	const unsigned long eax = features & 0xfffffffful;
	const unsigned long ecx = 0;

	__asm__ volatile ("xsetbv"
				:
				: "c"(ecx), "d"(edx), "a"(eax));
}

static void fpu_enumerate_features(void)
{
	unsigned long eax, ebx, ecx, edx;

	eax = 0x01; ecx = 0;
	cpuid(&eax, &ebx, &ecx, &edx);
	BUG_ON(!(ecx & CPUID_XSAVE));

	eax = 0x0d; ecx = 0;
	cpuid(&eax, &ebx, &ecx, &edx);
	xcr0_features = XSAVE_MASK(eax | ((unsigned long long)edx << 32));
}

static void fpu_enable_xsave(void)
{
	const unsigned long long cr4 = read_cr4();
	const unsigned long long cr0 = read_cr0();

	write_cr0((cr0 & ~(CR0_EM | CR0_TS)) | CR0_MP);
	write_cr4(cr4 | CR4_OSXSAVE | CR4_OSXMMEXCPT | CR4_OSFXSR);
	__xcr0_write(xcr0_features);
	__asm__ volatile ("fninit");
}

static void fpu_find_state_size(void)
{
	state_size = 512 + 64;

	for (int i = 2; i != sizeof(xcr0_features) * CHAR_BIT; ++i) {
		if ((xcr0_features & (1ull << i)) == 0)
			continue;

		unsigned long eax, ebx, ecx, edx;

		eax = 0xd; ecx = i;
		cpuid(&eax, &ebx, &ecx, &edx);
		if (ebx + eax > (unsigned long)state_size)
			state_size = ebx + eax;
	}
}

void fpu_cpu_setup(void)
{
	fpu_enumerate_features();
	fpu_enable_xsave();
	fpu_find_state_size();
}
