#ifndef __BACKTRACE_H__
#define __BACKTRACE_H__

#include <stdint.h>

void __backtrace(uintptr_t rbp, uintptr_t stack_begin, uintptr_t stack_end);
void backtrace(void);

#endif /*__BACKTRACE_H__*/
