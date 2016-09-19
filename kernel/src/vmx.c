#include <vmx.h>
#include <debug.h>
#include <memory.h>
#include <string.h>
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

#define SGA_A		(1 << 0)
#define SGA_W		(1 << 1)
#define SGA_R		(1 << 1)
#define SGA_X		(1 << 3)
#define SGA_NOT_SYSTEM	(1 << 4)
#define SGA_PRESENT	(1 << 7)
#define SGA_LONG	(1 << 13)
#define SGA_INVALID	(1 << 16)

#define __SGA_CODE	(SGA_NOT_SYSTEM | SGA_X)
#define __SGA_DATA	SGA_NOT_SYSTEM
#define __SGA_WA	(SGA_W | SGA_A)
#define __SGA_RA	(SGA_R | SGA_A)
#define __SGA_TSS_BUSY	11
#define __SGA_TSS_AVAIL	9

#define SGA_CODE_RA		(SGA_PRESENT | __SGA_CODE | __SGA_RA)
#define SGA_DATA_WA		(SGA_PRESENT | __SGA_DATA | __SGA_WA)
#define SGA_TSS_BUSY		(SGA_PRESENT | __SGA_TSS_BUSY)
#define SGA_CODE_RA_LONG	(SGA_CODE_RA | SGA_LONG)
#define SGA_DATA_WA_LONG	(SGA_DATA_WA | SGA_LONG)

int __vmcs_launch(struct vmx_guest_state *state);
int __vmcs_resume(struct vmx_guest_state *state);


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

static int __vmxon(unsigned long vmxon_addr)
{
	unsigned long rflags;

	__asm__ volatile ("vmxon %1; pushfq; popq %0"
		: "=rm"(rflags)
		: "m"(vmxon_addr)
		: "memory", "cc");
	return (rflags & RFLAGS_CF) ? -1 : 0;
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

	uintptr_t vmxon_addr;

	BUG_ON(!(vmxon_addr = page_alloc(1, PA_NORMAL)));
	vmxon_setup((volatile void *)vmxon_addr);

	write_cr4(read_cr4() | CR4_VMXE);
	vmxon_check();
	BUG_ON(__vmxon(vmxon_addr) < 0);
}

static int __vmcs_load(unsigned long vmcs)
{
	unsigned char err;

	__asm__ volatile ("vmptrld %1; setna %0"
		: "=r"(err)
		: "m"(vmcs)
		: "memory", "cc");
	return err ? -1 : 0;
}

static int __vmcs_store(unsigned long *vmcs)
{
	unsigned char err;

	__asm__ volatile ("vmptrst %1; setna %0"
		: "=r"(err), "=m"(*vmcs)
		:
		: "memory", "cc");
	return err ? -1 : 0;
}

static int __vmcs_clear(unsigned long vmcs)
{
	unsigned char err;

	__asm__ volatile ("vmclear %1; setna %0"
		: "=r"(err)
		: "m"(vmcs)
		: "memory", "cc");
	return err ? -1 : 0;
}

static int __vmcs_write(unsigned long field, unsigned long long val)
{
	unsigned char err;

	__asm__ volatile ("vmwrite %1, %2; setna %0"
		: "=r"(err)
		: "r"(val), "r"(field)
		: "memory", "cc");
	return err ? -1 : 0;
}

static int __vmcs_read(unsigned long field, unsigned long long *val)
{
	unsigned char err;

	__asm__ volatile ("vmread %2, %1; setna %0"
		: "=r"(err), "=r"(*val)
		: "r"(field)
		: "memory", "cc");
	return err ? -1 : 0;
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

static void vmcs_set_defctrls(void)
{
	const unsigned long long basic = read_msr(IA32_VMX_BASIC);
	const unsigned long long pinbased_ctls =
				read_msr(IA32_VMX_PINBASED_CTLS);
	const unsigned long long procbased_ctls =
				read_msr(IA32_VMX_PROCBASED_CTLS);
	const unsigned long long exit_ctls = read_msr(IA32_VMX_EXIT_CTLS);
	const unsigned long long entry_ctls = read_msr(IA32_VMX_ENTRY_CTLS);

	if (!(basic & (1ull << 5))) {
		__vmcs_write(VMCS_PINBASED_CTLS, vmcs_defctls(pinbased_ctls));
		__vmcs_write(VMCS_PROCBASED_CTLS, vmcs_defctls(procbased_ctls));
		__vmcs_write(VMCS_VMEXIT_CTLS, vmcs_defctls(exit_ctls));
		__vmcs_write(VMCS_VMENTRY_CTLS, vmcs_defctls(entry_ctls));
	} else {
		const unsigned long long true_pinbased_ctls =
					read_msr(IA32_VMX_TRUE_PINBASED_CTLS);
		const unsigned long long true_procbased_ctls =
					read_msr(IA32_VMX_TRUE_PROCBASED_CTLS);
		const unsigned long long true_exit_ctls =
					read_msr(IA32_VMX_TRUE_EXIT_CTLS);
		const unsigned long long true_entry_ctls =
					read_msr(IA32_VMX_TRUE_ENTRY_CTLS);

		__vmcs_write(VMCS_PINBASED_CTLS, __vmcs_defctls(pinbased_ctls,
					true_pinbased_ctls));
		__vmcs_write(VMCS_PROCBASED_CTLS, __vmcs_defctls(procbased_ctls,
					true_procbased_ctls));
		__vmcs_write(VMCS_VMEXIT_CTLS, __vmcs_defctls(exit_ctls,
					true_exit_ctls));
		__vmcs_write(VMCS_VMENTRY_CTLS, __vmcs_defctls(entry_ctls,
					true_entry_ctls));
	}
}

static uintptr_t vmcs_alloc(void)
{
	const uintptr_t vmcs = page_alloc(0, PA_NORMAL);
	volatile uint32_t *ptr = (volatile uint32_t *)vmcs;

	BUG_ON(!ptr);
	memset((void *)ptr, 0, PAGE_SIZE);
	ptr[0] = vmx_revision();
	ptr[1] = 0;

	return vmcs;
}

static void vmcs_free(uintptr_t vmcs)
{
	page_free(vmcs, 0);
}

void vmx_guest_setup(struct vmx_guest *guest)
{
	memset(guest, 0, sizeof(*guest));
}

void vmx_guest_release(struct vmx_guest *guest)
{
	vmcs_free(guest->vmcs);
	memset(guest, 0, sizeof(*guest));
}

static void vmx_host_state_setup(struct vmx_guest *guest)
{
	struct desc_ptr ptr;

	(void) guest;

	BUG_ON(__vmcs_write(VMCS_HOST_CR0, read_cr0()) < 0);
	BUG_ON(__vmcs_write(VMCS_HOST_CR3, read_cr3()) < 0);
	BUG_ON(__vmcs_write(VMCS_HOST_CR4, read_cr4()) < 0);
	BUG_ON(__vmcs_write(VMCS_HOST_ES, KERNEL_DATA) < 0);
	BUG_ON(__vmcs_write(VMCS_HOST_CS, KERNEL_CODE) < 0);
	BUG_ON(__vmcs_write(VMCS_HOST_SS, KERNEL_DATA) < 0);
	BUG_ON(__vmcs_write(VMCS_HOST_DS, KERNEL_DATA) < 0);
	BUG_ON(__vmcs_write(VMCS_HOST_TR, KERNEL_TSS) < 0);

	read_gdt(&ptr);
	BUG_ON(__vmcs_write(VMCS_HOST_GDTR_BASE, ptr.base) < 0);
	read_idt(&ptr);
	BUG_ON(__vmcs_write(VMCS_HOST_IDTR_BASE, ptr.base) < 0);
}

static void vmx_guest_state_setup(struct vmx_guest *guest)
{
	BUG_ON(__vmcs_write(VMCS_GUEST_RIP, guest->entry) < 0);
	BUG_ON(__vmcs_write(VMCS_GUEST_RSP, guest->stack) < 0);
	BUG_ON(__vmcs_write(VMCS_GUEST_RFLAGS, (1 << 1)) < 0);
	BUG_ON(__vmcs_write(VMCS_GUEST_CR0, read_cr0()) < 0);
	BUG_ON(__vmcs_write(VMCS_GUEST_CR3, read_cr3()) < 0);
	BUG_ON(__vmcs_write(VMCS_GUEST_CR4, read_cr4()) < 0);
	BUG_ON(__vmcs_write(VMCS_GUEST_DR7, 0) < 0);

	BUG_ON(__vmcs_write(VMCS_GUEST_ES_BASE, 0) < 0);
	BUG_ON(__vmcs_write(VMCS_GUEST_CS_BASE, 0) < 0);
	BUG_ON(__vmcs_write(VMCS_GUEST_SS_BASE, 0) < 0);
	BUG_ON(__vmcs_write(VMCS_GUEST_DS_BASE, 0) < 0);
	BUG_ON(__vmcs_write(VMCS_GUEST_TR_BASE, 0) < 0);

	BUG_ON(__vmcs_write(VMCS_GUEST_ES_LIMIT, 0) < 0);
	BUG_ON(__vmcs_write(VMCS_GUEST_CS_LIMIT, 0) < 0);
	BUG_ON(__vmcs_write(VMCS_GUEST_SS_LIMIT, 0) < 0);
	BUG_ON(__vmcs_write(VMCS_GUEST_DS_LIMIT, 0) < 0);
	BUG_ON(__vmcs_write(VMCS_GUEST_TR_LIMIT, 0) < 0);

	BUG_ON(__vmcs_write(VMCS_GUEST_CS_ACCESS, SGA_CODE_RA_LONG) < 0);
	BUG_ON(__vmcs_write(VMCS_GUEST_ES_ACCESS, SGA_DATA_WA_LONG) < 0);
	BUG_ON(__vmcs_write(VMCS_GUEST_SS_ACCESS, SGA_DATA_WA_LONG) < 0);
	BUG_ON(__vmcs_write(VMCS_GUEST_DS_ACCESS, SGA_DATA_WA_LONG) < 0);
	BUG_ON(__vmcs_write(VMCS_GUEST_TR_ACCESS, SGA_TSS_BUSY) < 0);
	BUG_ON(__vmcs_write(VMCS_GUEST_FS_ACCESS, SGA_INVALID) < 0);
	BUG_ON(__vmcs_write(VMCS_GUEST_GS_ACCESS, SGA_INVALID) < 0);
	BUG_ON(__vmcs_write(VMCS_GUEST_LDTR_ACCESS, SGA_INVALID) < 0);
}

static void vmx_guest_config_setup(struct vmx_guest *guest)
{
	unsigned long long conf;

	(void) guest;

	vmcs_set_defctrls();

	BUG_ON(__vmcs_read(VMCS_VMEXIT_CTLS, &conf) < 0);
	BUG_ON(__vmcs_write(VMCS_VMEXIT_CTLS,
				conf | VMCS_VMEXIT_CTLS_HOST_ADDR_SIZE) < 0);

	BUG_ON(__vmcs_read(VMCS_PINBASED_CTLS, &conf) < 0);
	BUG_ON(__vmcs_write(VMCS_PINBASED_CTLS,
				conf | VMCS_PINBASED_CTLS_INT_EXIT) < 0);

	BUG_ON(__vmcs_read(VMCS_VMENTRY_CTLS, &conf) < 0);
	BUG_ON(__vmcs_write(VMCS_VMENTRY_CTLS,
				conf | VMCS_VMENTRY_CTLS_IA32E_GUEST) < 0);

	BUG_ON(__vmcs_write(VMCS_LINK_PTR, 0xffffffffffffffffull) < 0);
}

static void vmx_guest_first_setup(struct vmx_guest *guest)
{
	guest->vmcs = vmcs_alloc();
	BUG_ON(__vmcs_clear(guest->vmcs) < 0);
	BUG_ON(__vmcs_load(guest->vmcs) < 0);

	vmx_guest_config_setup(guest);
	vmx_host_state_setup(guest);
	vmx_guest_state_setup(guest);
}

static void vmx_guest_setup_current(struct vmx_guest *guest)
{
	if (!guest->vmcs) {
		vmx_guest_first_setup(guest);
		return;
	}

	unsigned long vmcs;

	BUG_ON(__vmcs_store(&vmcs) < 0);
	if (vmcs != guest->vmcs) {
		guest->launched = 0;
		BUG_ON(__vmcs_clear(guest->vmcs) < 0);
		BUG_ON(__vmcs_load(guest->vmcs) < 0);
	}
}

void vmx_guest_run(struct vmx_guest *guest)
{
	vmx_guest_setup_current(guest);

	while (1) {
		local_int_disable();
		if (guest->launched) {
			BUG_ON(__vmcs_resume(&guest->state) < 0);
		} else {
			guest->launched = 1;
			BUG_ON(__vmcs_launch(&guest->state) < 0);
		}
		local_int_enable();
	}
}
