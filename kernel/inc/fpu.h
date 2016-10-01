#ifndef __FPU_H__
#define __FPU_H__

int fpu_state_size(void);
void fpu_state_save(void *state);
void fpu_state_restore(const void *state);
void fpu_state_setup(void *state);

void fpu_cpu_setup(void);

#endif /*__FPU_H__*/
