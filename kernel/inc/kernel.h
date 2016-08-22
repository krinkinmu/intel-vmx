#ifndef __KERNEL_H__
#define __KERNEL_H__

#include <stddef.h>

#define CONTAINER_OF(ptr, type, member) \
	(type *)( (char *)(ptr) - offsetof(type, member) )

#endif /*__KERNEL_H__*/
