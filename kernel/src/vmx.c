#include <vmx.h>
#include <debug.h>
#include <memory.h>
#include <cpu.h>


/* [0:31]  if 0, then corresponding pin control is allowed to be 0
   [32:63] if 1, then corresponding pin control (x-32) allowed to be 1 */
#define IA32_VMX_PINBASED_CTLS		0x481
#define IA32_VMX_TRUE_PINBASED_CTLS	0x48d
/* Same as IA32_VMX_PINBASED_CTLS, but for primary processor based controls */
#define IA32_VMX_PROCBASED_CTLS		0x482
#define IA32_VMX_TRUE_PROCBASED_CTLS	0x48e
/* Exists only if 63 bit of IA32_VMX_PROCBASED_CTLS is 1 */
#define IA32_VMX_PROCBASED_CTLS2	0x48b

#define IA32_VMX_EXIT_CTLS		0x483
#define IA32_VMX_TRUE_EXIT_CTLS		0x48F

#define IA32_VMX_ENTRY_CTLS		0x484
#define IA32_VMX_TRUE_ENTRY_CTLS	0x490

/* [0:30]  VMCS revision identifier
   [32:44] size of VMXON and VMCS regions in bytes (at most 4096 bytes and
           must be page aligned, so not very interesting)
   48      if 0 then every control structure memory limited by processor
           capabilities, oterwise every structure must be in first 4GB
   [50:53] memory type that should be used for control strucutres (page flags)
   54      if 1, then VM exits due to ins/outs are indicated in
           instruction-information field
   55      if 1 then VMX controls that are default to 1 may be cleared to 0 */
#define IA32_VMX_BASIC		0x480
#define IA32_VMX_MISC		0x485
#define IA32_VMX_CR0_FIXED0	0x486
#define IA32_VMX_CR0_FIXED1	0x487
#define IA32_VMX_CR4_FIXED0	0x488
#define IA32_VMX_CR4_FIXED1	0x489
#define IA32_VMX_VMCS_ENUM	0x48a
#define IA32_VMX_EPT_VPID	0x48c
#define IA32_VMX_VMFUNC		0x491
#define IA32_FEATURE_CONTROL	0x03a
#define IA32_FC_LOCK		(1ull << 0)
#define IA32_FC_NATIVE_VMX	(1ull << 2)

static uintptr_t vmxon_addr;


static unsigned long vmx_revision(void)
{
	const unsigned long long vmx_basic = read_msr(IA32_VMX_BASIC);

	return vmx_basic & ((1ul << 31) - 1);
}

static void vmxon_setup(volatile void *vmxon_ptr)
{
	volatile uint32_t *ptr = vmxon_ptr;

	*ptr = vmx_revision();
}

static void vmxon_check(void)
{
	const unsigned long long cr0_fixed0 = read_msr(IA32_VMX_CR0_FIXED0);
	const unsigned long long cr0_fixed1 = read_msr(IA32_VMX_CR0_FIXED1);
	const unsigned long long cr0 = read_cr0();

	BUG_ON((cr0_fixed0 & cr0) != cr0_fixed0);
	BUG_ON((~cr0_fixed1 & cr0) != 0);

	const unsigned long long cr4_fixed0 = read_msr(IA32_VMX_CR4_FIXED0);
	const unsigned long long cr4_fixed1 = read_msr(IA32_VMX_CR4_FIXED1);
	const unsigned long long cr4 = read_cr4();

	BUG_ON((cr4_fixed0 & cr4) != cr4_fixed0);
	BUG_ON((~cr4_fixed1 & cr4) != 0);
	BUG_ON((read_msr(IA32_FEATURE_CONTROL) & 1) != 1);
}

static int vmxon(unsigned long vmxon_addr)
{
	unsigned long rflags;

	__asm__ ("vmxon %1; pushfq; popq %0"
		: "=rm"(rflags)
		: "m"(vmxon_addr)
		: "memory", "cc");
	return (rflags & RFLAGS_CF) ? -1 : 0;
}

static int vmxoff(void)
{
	unsigned long rflags;

	__asm__ ("vmxoff; pushfq; popq %0"
		: "=rm"(rflags)
		:
		: "memory", "cc");
	return (rflags & (RFLAGS_CF | RFLAGS_ZF)) ? -1 : 0;
}

void vmx_enter(void)
{
	write_cr4(read_cr4() | CR4_VMXE);
	vmxon_check();
	BUG_ON(vmxon(vmxon_addr) < 0);
}

void vmx_exit(void)
{
	BUG_ON(vmxoff() < 0);
}

static int vmx_supported(void)
{
	unsigned long eax = 1, ebx, ecx, edx;

	cpuid(&eax, &ebx, &ecx, &edx);
	return (ecx & (1ul << 5)) ? 1 : 0;
}

void vmx_setup(void)
{
	BUG_ON(!vmx_supported());

	const unsigned long long feature = read_msr(IA32_FEATURE_CONTROL);
	const unsigned long long mask = IA32_FC_LOCK | IA32_FC_NATIVE_VMX;

	if ((feature & mask) != mask) {
		write_msr(IA32_FEATURE_CONTROL, feature | mask);
		BUG_ON((read_msr(IA32_FEATURE_CONTROL) & mask) != mask);
	}

	BUG_ON(!(vmxon_addr = page_alloc(1, PA_LOW_MEM)));
	vmxon_setup((volatile void *)vmxon_addr);
}

uintptr_t vmcs_alloc(void)
{
	const uintptr_t vmcs = page_alloc(0, PA_LOW_MEM);
	volatile uint32_t *ptr = (volatile uint32_t *)vmcs;

	BUG_ON(!ptr);
	ptr[0] = vmx_revision();
	ptr[1] = 0;
	BUG_ON(vmcs_release(vmcs) < 0);

	return vmcs;
}

void vmcs_free(uintptr_t vmcs)
{
	page_free(vmcs, 0);
}

int vmcs_setup(uintptr_t vmcs)
{
	unsigned long rflags;

	__asm__ ("vmptrld %1; pushfq; pop %0"
		: "=m"(rflags)
		: "m"(vmcs)
		: "memory", "cc");
	return (rflags & (RFLAGS_CF | RFLAGS_ZF)) ? -1 : 0;
}

static unsigned long ___vmcs_defctls(unsigned long low,
			unsigned long tlow, unsigned long thigh)
{
	const unsigned long var = ~tlow & thigh;

	return (tlow & ~var) | (low & var);
}

static unsigned long __vmcs_defctls(unsigned long long ctls,
			unsigned long long true_ctls)
{
	const unsigned long tlow = true_ctls & 0xfffffffful;
	const unsigned long thigh = (true_ctls >> 32) & 0xfffffffful;
	const unsigned long low = ctls & 0xfffffffful;

	return ___vmcs_defctls(low, tlow, thigh);
}

static unsigned long vmcs_defctls(unsigned long long ctls)
{
	return __vmcs_defctls(ctls, 0xffffffffull << 32);
}

void vmcs_reset(void)
{
	const unsigned long long basic = read_msr(IA32_VMX_BASIC);
	const unsigned long long pinbased_ctls =
				read_msr(IA32_VMX_PINBASED_CTLS);
	const unsigned long long procbased_ctls =
				read_msr(IA32_VMX_PROCBASED_CTLS);
	const unsigned long long exit_ctls = read_msr(IA32_VMX_EXIT_CTLS);
	const unsigned long long entry_ctls = read_msr(IA32_VMX_ENTRY_CTLS);

	if (!(basic & (1ull << 5))) {
		vmcs_write(VMCS_PINBASED_CTLS, vmcs_defctls(pinbased_ctls));
		vmcs_write(VMCS_PROCBASED_CTLS, vmcs_defctls(procbased_ctls));
		vmcs_write(VMCS_EXIT_CTLS, vmcs_defctls(exit_ctls));
		vmcs_write(VMCS_ENTRY_CTLS, vmcs_defctls(entry_ctls));
	} else {
		const unsigned long long true_pinbased_ctls =
					read_msr(IA32_VMX_TRUE_PINBASED_CTLS);
		const unsigned long long true_procbased_ctls =
					read_msr(IA32_VMX_TRUE_PROCBASED_CTLS);
		const unsigned long long true_exit_ctls =
					read_msr(IA32_VMX_TRUE_EXIT_CTLS);
		const unsigned long long true_entry_ctls =
					read_msr(IA32_VMX_TRUE_ENTRY_CTLS);

		vmcs_write(VMCS_PINBASED_CTLS, __vmcs_defctls(pinbased_ctls,
					true_pinbased_ctls));
		vmcs_write(VMCS_PROCBASED_CTLS, __vmcs_defctls(procbased_ctls,
					true_procbased_ctls));
		vmcs_write(VMCS_EXIT_CTLS, __vmcs_defctls(exit_ctls,
					true_exit_ctls));
		vmcs_write(VMCS_ENTRY_CTLS, __vmcs_defctls(entry_ctls,
					true_entry_ctls));
	}
}

int vmcs_release(uintptr_t vmcs)
{
	unsigned long rflags;

	__asm__ ("vmclear %1; pushfq; pop %0"
		: "=m"(rflags)
		: "m"(vmcs)
		: "memory", "cc");
	return (rflags & (RFLAGS_CF | RFLAGS_ZF)) ? -1 : 0;
}

void vmcs_write(unsigned long field, unsigned long long val)
{
	unsigned long rflags;

	__asm__ ("vmwrite %1, %2; pushfq; pop %0"
		: "=r"(rflags)
		: "r"(val), "r"(field)
		: "memory", "cc");
	BUG_ON((rflags & (RFLAGS_CF | RFLAGS_ZF)));
}

unsigned long long vmcs_read(unsigned long field)
{
	unsigned long long val;
	unsigned long rflags;

	__asm__ ("vmread %2, %1; pushfq; pop %0"
		: "=r"(rflags), "=r"(val)
		: "r"(field)
		: "memory", "cc");
	BUG_ON((rflags & (RFLAGS_CF | RFLAGS_ZF)));
	return val;
}
