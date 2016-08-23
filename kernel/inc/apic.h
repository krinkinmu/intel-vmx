#ifndef __APIC_H__
#define __APIC_H__

extern int local_apics;
extern int local_apic_ids[];

unsigned long local_apic_read(int reg);
void local_apic_write(int reg, unsigned long val);
int local_apic_version(void);
int local_apic_id(void);

void apic_setup(void);

#endif /*__APIC_H__*/
