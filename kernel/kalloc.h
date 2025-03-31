#ifndef KALLOC_H
#define KALLOC_H

#include "spinlock.h"  // Assuming spinlock.h defines struct spinlock

struct ref_stru {
  struct spinlock lock;
  int cnt[PHYSTOP / PGSIZE];  // 引用计数
};

extern struct ref_stru ref;

#endif
