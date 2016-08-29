#ifndef __TIME_H__
#define __TIME_H__

#define TIMER_TICK	1000
#define TIMER_IRQ	0

void time_setup(void);
void udelay(unsigned long usec);
unsigned long long current_time(void);

#endif /*__TIME_H__*/
