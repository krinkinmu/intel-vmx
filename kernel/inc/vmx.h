#ifndef __VMX_H__
#define __VMX_H__

#include <stdint.h>

#define VMCS_ACCESS_TYPE(x)	((unsigned long)(x) << 0)
#define VMCS_ACCESS_FULL	VMCS_ACCESS_TYPE(0)
#define VMCS_ACCESS_HIGH	VMCS_ACCESS_TYPE(1)

#define VMCS_INDEX(x)		(((unsigned long)(x) & 0x1ff) << 1)

#define VMCS_WIDTH(x)		(((unsigned long)(x) & 3) << 13)
#define VMCS_WIDTH_16		VMCS_WIDTH(0)
#define VMCS_WIDTH_64		VMCS_WIDTH(1)
#define VMCS_WIDTH_32		VMCS_WIDTH(2)
#define VMCS_WIDTH_NATIVE	VMCS_WIDTH(3)

#define VMCS_PINBASED_CTLS	(VMCS_WIDTH(32) | VMCS_INDEX(0))
#define VMCS_PROCBASED_CTLS	(VMCS_WIDTH(32) | VMCS_INDEX(1))
#define VMCS_EXIT_CTLS		(VMCS_WIDTH(32) | VMCS_INDEX(6))
#define VMCS_ENTRY_CTLS		(VMCS_WIDTH(32) | VMCS_INDEX(9))

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

uintptr_t vmcs_alloc(void);
void vmcs_free(uintptr_t vmcs);

void vmcs_reset(void);
int vmcs_setup(uintptr_t vmcs);
int vmcs_release(uintptr_t vmcs);

int vmcs_launch(struct vmx_guest_state *state);
int vmcs_resume(struct vmx_guest_state *state);

void vmcs_write(unsigned long field, unsigned long long val);
unsigned long long vmcs_read(unsigned long field);

void vmx_setup(void);
void vmx_enter(void);
void vmx_exit(void);

#endif /*__VMX_H__*/
