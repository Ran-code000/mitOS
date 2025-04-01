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
extern struct ref_count ref; // 声明全局变量

struct kmem {
  struct spinlock lock;
  struct run *freelist;
};
extern struct kmem kmem; // 声明全局变量
# endif
