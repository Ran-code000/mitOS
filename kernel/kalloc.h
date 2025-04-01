<<<<<<< Updated upstream
#ifndef KALLOC_H
#define KALLOC_H

#include "spinlock.h"  // Assuming spinlock.h defines struct spinlock

struct ref_stru {
  struct spinlock lock;
  int cnt[PHYSTOP / PGSIZE];  // 引用计数
};

extern struct ref_stru ref;

#endif
=======
# ifndef KALLOC_H
# define KALLOC_H

#include "spinlock.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct ref_count{
  struct spinlock lock;
  int cnt[PHYSTOP / PGSIZE];
};

struct kmem {
  struct spinlock lock;
  struct run *freelist;
};

# endif
>>>>>>> Stashed changes
