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

uintptr_t vmcs_alloc(void);
void vmcs_free(uintptr_t vmcs);

int vmcs_setup(uintptr_t vmcs);
int vmcs_release(uintptr_t vmcs);

int vmcs_launch(void);
int vmcs_resume(void);

void vmcs_write(unsigned long field, unsigned long long val);
unsigned long long vmcs_read(unsigned long field);

void vmx_setup(void);
void vmx_enter(void);
void vmx_exit(void);

#endif /*__VMX_H__*/
