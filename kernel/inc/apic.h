#ifndef __APIC_H__
#define __APIC_H__

#define APIC_ICR_INIT		(5ul << 8)
#define APIC_ICR_STARTUP	(6ul << 8)
#define APIC_ICR_LOGICAL	(1ul << 11)
#define APIC_ICR_PHYSCAL	(0ul << 11)
#define APIC_ICR_PENDING	(1ul << 12)
#define APIC_ICR_ASSERT		(1ul << 14)
#define APIC_ICR_DEASSERT	(0ul << 14)
#define APIC_ICR_LEVEL		(1ul << 15)
#define APIC_ICR_EDGE		(0ul << 15)

extern int local_apics;
extern int local_apic_ids[];

unsigned long local_apic_read(int reg);
void local_apic_write(int reg, unsigned long val);
int local_apic_version(void);
int local_apic_id(void);
void local_apic_icr_write(int dest, unsigned long flags);

void apic_setup(void);

#endif /*__APIC_H__*/
