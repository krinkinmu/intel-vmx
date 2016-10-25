#ifndef __PER_CPU_H__
#define __PER_CPU_H__

/* According to ABI __thread puts variable into one of specialized sections
 * (.tbss or .tdata depending on whether variable is initialized), then we can
 * use those sections as an initial image of percpu data. We can achieve the
 * same effect with section attribute, but it only works for initialized data,
 * so we would need to combine it with something else, for example weak
 * attribute, so i use this dirty hack with TLS, since it seems simpler. */
#define __percpu	_Thread_local

void percpu_cpu_setup(void);
void percpu_setup(void);

#endif /*__PER_CPU_H__*/
