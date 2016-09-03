#ifndef __VMX_H__
#define __VMX_H__

#include <stdint.h>

#define VMCS_VPID		0x0ul
#define VMCS_POSTED_INT_VECTOR	0x2ul
#define VMCS_EPTP_INDEX		0x4ul

#define VMCS_GUEST_ES		0x800ul
#define VMCS_GUEST_CS		0x802ul
#define VMCS_GUEST_SS		0x804ul
#define VMCS_GUEST_DS		0x806ul
#define VMCS_GUEST_FS		0x808ul
#define VMCS_GUEST_GS		0x80aul
#define VMCS_GUEST_LDTR		0x80cul
#define VMCS_GUEST_TR		0x80eul
#define VMCS_GUEST_INT_STATUS	0x810ul
#define VMCS_GUEST_PML_INDEX	0x812ul

#define VMCS_HOST_ES	0xc00ul
#define VMCS_HOST_CS	0xc02ul
#define VMCS_HOST_SS	0xc04ul
#define VMCS_HOST_DS	0xc06ul
#define VMCS_HOST_FS	0xc08ul
#define VMCS_HOST_GS	0xc0aul
#define VMCS_HOST_TR	0xc0cul

#define VMCS_IO_BITMAP_A_ADDR		0x2000ul
#define VMCS_IO_BITMAP_B_ADDR		0x2002ul
#define VMCS_MSR_BITMAP_ADDR		0x2004ul
#define VMCS_VMEXIT_MSR_STORE_ADDR	0x2006ul
#define VMCS_VMEXIT_MSR_LOAD_ADDR	0x2008ul
#define VMCS_VMENTRY_MSR_LOAD_ADDR	0x200aul
#define VMCS_EXECUTIVE_VMCS_PTR		0x200cul
#define VMCS_PML_ADDR			0x200eul
#define VMCS_TSC_OFFS			0x2010ul
#define VMCS_VAPIC_ADDR			0x2012ul
#define VMCS_APIC_ACCESS_ADDR		0x2014ul
#define VMCS_POSTED_INT_DESC_ADDR	0x2016ul
#define VMCS_VMFUNC_CTLS		0x2018ul
#define VMCS_EPT_PTR			0x201aul
#define VMCS_EOI_EXIT_BITMAP0		0x201cul
#define VMCS_EOI_EXIT_BITMAP1		0x201eul
#define VMCS_EOI_EXIT_BITMAP2		0x2020ul
#define VMCS_EOI_EXIT_BITMAP3		0x2022ul
#define VMCS_EPTP_LIST_ADDR		0x2024ul
#define VMCS_VMREAD_BITMAP_ADDR		0x2026ul
#define VMCS_VMWRITE_BITMAP_ADDR	0x2028ul
#define VMCS_VIRT_EXCEPTION_INFO_ADDR	0x202aul
#define VMCS_XSS_EXIT_BITMAP		0x202cul
#define VMCS_ENCLS_EXIT_BITMAP		0x202eul
#define VMCS_TSC_MUL			0x2032ul

#define VMCS_GUEST_PHYS_ADDR			0x2400ul
#define VMCS_LINK_PTR				0x2800ul
#define VMCS_GUEST_IA32_DEBUGCTL		0x2800ul
#define VMCS_GUEST_IA32_PAT			0x2804ul
#define VMCS_GUEST_EFER				0x2806ul
#define VMCS_GUEST_IA32_PERF_GLOBAL_CTRL	0x2808ul
#define VMCS_GUEST_PDPTE0			0x280aul
#define VMCS_GUEST_PDPTE1			0x280cul
#define VMCS_GUEST_PDPTE2			0x280eul
#define VMCS_GUEST_PDPTE3			0x2810ul
#define VMCS_GUEST_IA32_BNDCFGS			0x2812ul

#define VMCS_HOST_IA32_PAT			0x2c00ul
#define VMCS_HOST_IA32_EFER			0x2c02ul
#define VMCS_HOST_IA32_PERF_GLOBAL_CTRL		0x2c04ul

#define VMCS_PINBASED_CTLS		0x4000ul
#define VMCS_PROCBASED_CTLS		0x4002ul
#define VMCS_EXCEPTION_BITMAP		0x4004ul
#define VMCS_PAGE_FAULT_MASK		0x4006ul
#define VMCS_PAGE_FAULT_MATCH		0x4008ul
#define VMCS_CR3_TARGET_COUNT		0x400aul
#define VMCS_VMEXIT_CTLS		0x400cul
#define VMCS_VMEXIT_MSR_STORE_COUNT	0x400eul
#define VMCS_VMEXIT_MSR_LOAD_COUNT	0x4010ul
#define VMCS_VMENTRY_CTLS		0x4012ul
#define VMCS_VMENTRY_MSR_LOAD_COUNT	0x4014ul
#define VMCS_VMENTRY_INT_INFO		0x4016ul
#define VMCS_VMENTRY_EXCEPTION_CODE	0x4018ul
#define VMCS_VMENTRY_INST_LENGTH	0x401aul
#define VMCS_TPR_THRESHOLD		0x401cul
#define VMCS_PROCBASED_CTLS2		0x401eul
#define VMCS_PLE_GAP			0x4020ul
#define VMCS_PLE_WINDOW			0x4022ul

#define VMCS_VM_INTR_ERROR	0x4400ul
#define VMCS_EXIT_REASON	0x4402ul
#define VMCS_VMEXIT_INT_INFO	0x4404ul
#define VMCS_VMEXIT_INT_ERROR	0x4406ul
#define VMCS_IDT_INFO		0x4408ul
#define VMCS_IDT_ERROR		0x440aul
#define VMCS_VMEXIT_INST_LENGTH	0x440cul
#define VNCS_VNEXIT_INST_INFO	0x440eul

#define VMCS_GUEST_ES_LIMIT		0x4800ul
#define VMCS_GUEST_CS_LIMIT		0x4802ul
#define VMCS_GUEST_SS_LIMIT		0x4804ul
#define VMCS_GUEST_DS_LIMIT		0x4806ul
#define VMCS_GUEST_FS_LIMIT		0x4808ul
#define VMCS_GUEST_GS_LIMIT		0x480aul
#define VMCS_GUEST_LDTR_LIMIT		0x480cul
#define VMCS_GUEST_TR_LIMIT		0x480eul
#define VMCS_GUEST_GDTR_LIMIT		0x4810ul
#define VMCS_GUEST_IDTR_LIMIT		0x4812ul
#define VMCS_GUEST_ES_ACCESS		0x4814ul
#define VMCS_GUEST_CS_ACCESS		0x4816ul
#define VMCS_GUEST_SS_ACCESS		0x4818ul
#define VMCS_GUEST_DS_ACCESS		0x481aul
#define VMCS_GUEST_FS_ACCESS		0x481cul
#define VMCS_GUEST_GS_ACCESS		0x481eul
#define VMCS_GUEST_LDTR_ACCESS		0x4820ul
#define VMCS_GUEST_TR_ACCESS		0x4822ul
#define VMCS_GUEST_INT_STATE		0x4824ul
#define VMCS_GUEST_ACTIVE_STATE		0x4826ul
#define VMCS_GUEST_SMBASE		0x4828ul
#define VMCS_GUEST_IA32_SYSENTER_CS	0x482aul
#define VMCS_VMX_PREEMT_TIMER_VALUE	0x482eul

#define VMCS_HOST_IA32_SYSENTER_CS	0x4c00ul

#define VMCS_CR0_MASK		0x6000ul
#define VMCS_CR4_MASK		0x6002ul
#define VMCS_CR0_SHADOW		0x6004ul
#define VMCS_CR4_SHADOW		0x6006ul
#define VMCS_CR3_TARGET0	0x6008ul
#define VMCS_CR3_TARGET1	0x600aul
#define VMCS_CR3_TARGET2	0x600cul
#define VMCS_CR3_TARGET3	0x600eul

#define VMCS_EXIT_QUALIFICATION	0x6400ul
#define VMCS_IO_RCX		0x6402ul
#define VMCS_IO_RSI		0x6404ul
#define VMCS_IO_RDI		0x6406ul
#define VMCS_IO_RIP		0x6408ul
#define VMCS_GUEST_LINEAR_ADDR	0x640aul

#define VMCS_GUEST_CR0				0x6800ul
#define VMCS_GUEST_CR3				0x6802ul
#define VMCS_GUEST_CR4				0x6804ul
#define VMCS_GUEST_ES_BASE			0x6806ul
#define VMCS_GUEST_CS_BASE			0x6808ul
#define VMCS_GUEST_SS_BASE			0x680aul
#define VMCS_GUEST_DS_BASE			0x680cul
#define VMCS_GUEST_FS_BASE			0x680eul
#define VMCS_GUEST_GS_BASE			0x6810ul
#define VMCS_GUEST_LDTR_BASE			0x6812ul
#define VMCS_GUEST_TR_BASE			0x6814ul
#define VMCS_GUEST_GDTR_BASE			0x6816ul
#define VMCS_GUEST_IDTR_BASE			0x6818ul
#define VMCS_GUEST_DR7				0x681aul
#define VMCS_GUEST_RSP				0x681cul
#define VMCS_GUEST_RIP				0x681eul
#define VMCS_GUEST_RFLAGS			0x6820ul
#define VMCS_GUEST_PEDNING_DEBUG_EXCEPTIONS	0x6820ul
#define VMCS_GUEST_IA32_SYSENTER_ESP		0x6824ul
#define VMCS_GUEST_IA32_SYSENTER_RIP		0x6826ul

#define VMCS_HOST_CR0			0x6c00ul
#define VMCS_HOST_CR3			0x6c02ul
#define VMCS_HOST_CR4			0x6c04ul
#define VMCS_HOST_FS_BASE		0x6c06ul
#define VMCS_HOST_GS_BASE		0x6c08ul
#define VMCS_HOST_TR_BASE		0x6c0aul
#define VMCS_HOST_GDTR_BASE		0x6c0cul
#define VMCS_HOST_IDTR_BASE		0x6c0eul
#define VMCS_HOST_IA32_SYSENTER_ESP	0x6c10ul
#define VMCS_HOST_IA32_SYSENTER_EIP	0x6c12ul
#define VMCS_HOST_RSP			0x6c14ul
#define VMCS_HOST_RIP			0x6c16ul

struct vmx_guest_state {
	uint64_t rax;
	uint64_t rbx;
	uint64_t rcx;
	uint64_t rbp;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t r8;
	uint64_t r9;
	uint64_t r10;
	uint64_t r11;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;
} __attribute__((packed));

struct vmx_guest {
	struct vmx_guest_state state;
	uintptr_t vmcs;
	int launched;
	uintptr_t entry;
	uintptr_t stack;
};

void vmx_setup(void);
void vmx_enter(void);
void vmx_exit(void);

void vmx_guest_setup(struct vmx_guest *guest);
void vmx_guest_run(struct vmx_guest *guest);
void vmx_guest_release(struct vmx_guest *guest);

#endif /*__VMX_H__*/
