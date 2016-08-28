#ifndef __APIC_H__
#define __APIC_H__

#include <stdint.h>

#define APIC_ICR_INIT		(5ul << 8)
#define APIC_ICR_STARTUP	(6ul << 8)
#define APIC_ICR_LOGICAL	(1ul << 11)
#define APIC_ICR_PHYSCAL	(0ul << 11)
#define APIC_ICR_PENDING	(1ul << 12)
#define APIC_ICR_ASSERT		(1ul << 14)
#define APIC_ICR_DEASSERT	(0ul << 14)
#define APIC_ICR_LEVEL		(1ul << 15)
#define APIC_ICR_EDGE		(0ul << 15)

#define IO_APIC_VECTOR(x)	(x & 0xfful)

#define IO_APIC_DELIVERY_MODE(x)	((x & 0x7ul) << 8)
#define IO_APIC_FIXED			IO_APIC_DELIVERY_MODE(0)
#define IO_APIC_LOWEST			IO_APIC_DELIVERY_MODE(1)

#define IO_APIC_DESTINATION_MODE(x)	((x & 0x1ul) << 11)
#define IO_APIC_PHYSICAL		IO_APIC_DESTINATION_MODE(0)
#define IO_APIC_LOGICAL			IO_APIC_DESTINATION_MODE(1)

#define IO_APIC_POLARITY(x)	((x & 0x1ul) << 13)
#define IO_APIC_ACTIVE_HIGH	IO_APIC_POLARITY(0)
#define IO_APIC_ACTIVE_LOW	IO_APIC_POLARITY(1)

#define IO_APIC_TRIGGER(x)	((x & 0x1ul) << 15)
#define IO_APIC_EDGE		IO_APIC_TRIGGER(0)
#define IO_APIC_LEVEL		IO_APIC_TRIGGER(1)

#define IO_APIC_MASK_PIN	(1ul << 16)

#define IO_APIC_DESTINATION(x)	((x & 0xfful) << 24)
#define IO_APIC_RDREG(x)	(0x10 + 2 * (x))
#define IO_APIC_RDREG_LOW(x)	IO_APIC_RDREG(x)
#define IO_APIC_RDREG_HIGH(x)	(IO_APIC_RDREG(x) + 1)

struct io_apic {
	uintptr_t phys;
	int base_gsi;
	int pins;
};

extern struct io_apic ioapic[];
extern int ioapics;

extern int local_apics;
extern int local_apic_ids[];

unsigned long local_apic_read(int reg);
void local_apic_write(int reg, unsigned long val);
int local_apic_version(void);
int local_apic_id(void);
void local_apic_icr_write(int dest, unsigned long flags);

unsigned long io_apic_read(const struct io_apic *apic, int reg);
void io_apic_write(struct io_apic *apic, int reg, unsigned long val);

struct io_apic *io_apic_find(int gsi);
int io_apic_pin(const struct io_apic *apic, int gsi);

void apic_setup(void);

#endif /*__APIC_H__*/
