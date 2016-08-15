#include <acpi.h>

ACPI_STATUS AcpiOsInitialize(void)
{
	return AE_OK;
}

ACPI_STATUS AcpiOsTerminate(void)
{
	return AE_OK;
}

ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer(void)
{
	ACPI_PHYSICAL_ADDRESS phys = 0;

	AcpiFindRootPointer(&phys);
	return phys;
}

ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *predefined,
			ACPI_STRING *new_value)
{
	(void) predefined;
	*new_value = NULL;
	return AE_OK;
}

ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER *old_header,
			ACPI_TABLE_HEADER **new_table)
{
	(void) old_header;
	*new_table = NULL;
	return AE_OK;
}

ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER *old_header,
			ACPI_PHYSICAL_ADDRESS *new_phys, UINT32 *new_size)
{
	(void) old_header;
	*new_phys = 0;
	*new_size = 0;
	return AE_OK;
}

ACPI_STATUS AcpiOsCreateCache(char *name, UINT16 obj_size, UINT16 max_count,
			ACPI_CACHE_T **cachep)
{
	/* TODO: implement */
	(void) name;
	(void) obj_size;
	(void) max_count;
	*cachep = NULL;
	return AE_NO_MEMORY;
}

ACPI_STATUS AcpiOsDeleteCache(ACPI_CACHE_T *cache)
{
	/* TODO: implement */
	(void) cache;
	return AE_OK;
}

ACPI_STATUS AcpiOsPurgeCache(ACPI_CACHE_T *cache)
{
	(void) cache;
	return AE_OK;
}

void *AcpiOsAcquireObject(ACPI_CACHE_T *cache)
{
	(void) cache;
	return NULL;
}

ACPI_STATUS AcpiOsReleaseObject(ACPI_CACHE_T *cache, void *obj)
{
	(void) cache;
	(void) obj;
	return AE_OK;
}

void *AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS phys, ACPI_SIZE size)
{
	return (void *)phys;
}

void AcpiOsUnmapMemory(void *addr, ACPI_SIZE size)
{
	(void) addr;
	(void) size;
}

void *AcpiOsAllocate(ACPI_SIZE size)
{
	/* TODO: implement */
	(void) size;
	return NULL;
}

void AcpiOsFree(void *addr)
{
	/* TODO: implement */
	(void) addr;
}

ACPI_THREAD_ID AcpiOsGetThreadId(void)
{
	return 1;
}

ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE type, ACPI_OSD_EXEC_CALLBACK fptr,
			void *data)
{
	/* TODO: implement */
	(void) type;
	(void) fptr;
	(void) data;
	return AE_OK;
}

void AcpiOsSleep(UINT64 ms)
{
	/* TODO: implement */
}

void AcpiOsStall(UINT32 us)
{
	/* TODO: implement */
}

void AcpiOsWaitEventsComplete(void)
{
	/* TODO: implement */
}

ACPI_STATUS AcpiOsCreateSemaphore(UINT32 max, UINT32 init, ACPI_SEMAPHORE *sem)
{
	/* TODO: implement */
	(void) max;
	(void) init;
	(void) sem;
	return AE_NO_MEMORY;
}

ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE sem)
{
	/* TODO: implement */
	(void) sem;
	return AE_OK;
}

ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE sem, UINT32 acquire, UINT16 ms)
{
	/* TODO: implement */
	(void) sem;
	(void) acquire;
	(void) ms;
	return AE_OK;
}

ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE sem, UINT32 release)
{
	/* TODO: implement */
	(void) sem;
	(void) release;
	return AE_OK;
}

ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK *lock)
{
	/* TODO: implement */
	(void) lock;
	return AE_NO_MEMORY;
}

void AcpiOsDeleteLock(ACPI_SPINLOCK lock)
{
	/* TODO: implement */
	(void) lock;
}

ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK lock)
{
	/* TODO: implement */
	(void) lock;
	return 0;
}

void AcpiOsReleaseLock(ACPI_SPINLOCK lock, ACPI_CPU_FLAGS flags)
{
	/* TODO: implement */
	(void) lock;
	(void) flags;
}

ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 intno, ACPI_OSD_HANDLER fptr,
			void *data)
{
	/* TODO: implement */
	(void) intno;
	(void) fptr;
	(void) data;
	return AE_ALREADY_EXISTS;
}

ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 intno, ACPI_OSD_HANDLER fptr)
{
	/* TODO: implement */
	(void) intno;
	(void) fptr;
	return AE_NOT_EXIST;
}

ACPI_STATUS AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS phys, UINT64 *val,
			UINT32 width)
{
	const void *src = (const void *)phys;

	switch (width) {
	case 8:
		*val = *((uint8_t *)src);
		break;
	case 16:
		*val = *((uint16_t *)src);
		break;
	case 32:
		*val = *((uint32_t *)src);
		break;
	case 64:
		*val = *((uint64_t *)src);
		break;
	}
	return AE_OK;
}

ACPI_STATUS AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS phys, UINT64 val,
			UINT32 width)
{
	void *dst = (void *)phys;

	switch (width) {
	case 8:
		*((uint8_t *)dst) = (uint8_t)(val & 0xffULL);
		break;
	case 16:
		*((uint16_t *)dst) = (uint16_t)(val & 0xffffULL);
		break;
	case 32:
		*((uint32_t *)dst) = (uint32_t)(val & 0xffffffffULL);
		break;
	case 64:
		*((uint64_t *)dst) = (uint64_t)(val & 0xffffffffffffffffULL);
		break;
	}
	return AE_OK;
}

ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS ioport, UINT32 *val, UINT32 width)
{
	/* TODO: implement */
	(void) ioport;
	(void) val;
	(void) width;
	return AE_OK;
}

ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS ioport, UINT32 val, UINT32 width)
{
	/* TODO: implement */
	(void) ioport;
	(void) val;
	(void) width;
	return AE_OK;
}

ACPI_STATUS AcpiOsReadPciConfiguration(ACPI_PCI_ID *pci_id, UINT32 reg,
			UINT64 *val, UINT32 width)
{
	/* TODO: implement */
	(void) pci_id;
	(void) reg;
	(void) val;
	(void) width;
	return AE_OK;
}

ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID *pci_id, UINT32 reg,
			UINT64 val, UINT32 width)
{
	/* TODO: implement */
	(void) pci_id;
	(void) reg;
	(void) val;
	(void) width;
	return AE_OK;
}

void ACPI_INTERNAL_VAR_XFACE AcpiOsPrintf(const char *fmt, ...)
{
	/* TODO: implement */
}

void AcpiOsVprintf(const char *fmt, va_list args)
{
	/* TODO: implement */
}

UINT64 AcpiOsGetTimer(void)
{
	/* TODO: implement */
	return 0;
}

ACPI_STATUS AcpiOsSignal(UINT32 signo, void *info)
{
	(void) signo;
	(void) info;
	return AE_OK;
}
