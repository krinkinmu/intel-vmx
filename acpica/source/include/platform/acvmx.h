#ifndef __ACVMX_H__
#define __ACVMX_H__

#include <stdint.h>

#define ACPI_UINTPTR_T			uintptr_t
#define COMPILER_DEPENDENT_INT64	int64_t
#define COMPILER_DEPENDENT_UINT64	uint64_t

#define ACPI_MACHINE_WIDTH	64

#define ACPI_USE_SYSTEM_CLIBRARY
#define ACPI_USE_STANDARD_HEADERS

#undef ACPI_USE_LOCAL_CACHE
#undef ACPI_DEBUGGER
#undef ACPI_DISASSEMBLER

#endif /*__ACVMX_H___*/
