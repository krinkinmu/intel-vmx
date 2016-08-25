#ifndef __CPU_H__
#define __CPU_H__

static inline void cpu_relax(void)
{ __asm__ volatile("pause"); }

#endif /*__CPU_H__*/
