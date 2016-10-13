#ifndef __HAZARD_PTR_H__
#define __HAZARD_PTR_H__

#include <stdatomic.h>
#include <list.h>
#include <cpu.h>

#define HP_PTRS		32

struct hp_cpu_context;
struct hp_block;

struct hp_rec {
	void * _Atomic ptr;
	struct hp_block *owner;
	int free;
};

struct hp_block {
	struct list_head ll;
	struct hp_block *next;
	struct hp_cpu_context *cpu_ctx;
	struct hp_rec hp_rec[HP_PTRS];
	int free;
};

struct hp_deleted;
struct hp_protected;

struct hp_rlist {
	struct hp_deleted *deleted;
	int size, capacity, order;
};

struct hp_cpu_context {
	struct list_head full;
	struct list_head ready;
	struct hp_rlist rlist;
};

struct hp_domain {
	struct hp_block * _Atomic blocks;
	struct hp_cpu_context cpu_ctx[MAX_CPU_NR];
};

void hp_setup_domain(struct hp_domain *domain);
struct hp_rec *__hp_acquire(struct hp_domain *domain);
struct hp_rec *hp_acquire(void);
void hp_release(struct hp_rec *hp_rec);

void *hp_get(struct hp_rec *hp_rec);
void hp_set(struct hp_rec *hp_rec, void *ptr);
void hp_clear(struct hp_rec *hp_rec);

typedef void (*hp_free_fptr_t)(void *ptr, void *arg);
void __hp_retire(struct hp_domain *domain, void *ptr, hp_free_fptr_t free,
			void *arg);
void hp_retire(void *ptr, hp_free_fptr_t free, void *arg);

void __hp_gc(struct hp_domain *domain);
void hp_gc(void);

void hp_cpu_setup(void);
void hp_setup(void);

#endif /*__HAZARD_PTR_H__*/
