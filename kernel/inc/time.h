#ifndef __TIME_H__
#define __TIME_H__

/* NOTE: don't make it too large, because, for example, with value of 1000
   interrupts happens more often than requested, may be bug somewhere in
   QEMU hpet implementation? */
#define TIMER_TICK	1
#define TIMER_IRQ	0
#define TIMER_LOCAL_IRQ	1


void time_setup(void);
void time_cpu_setup(void);
void udelay(unsigned long usec);
unsigned long long current_time(void);

#endif /*__TIME_H__*/
