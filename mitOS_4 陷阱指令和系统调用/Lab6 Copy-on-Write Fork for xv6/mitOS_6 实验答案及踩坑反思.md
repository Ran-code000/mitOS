# Lab6: Copy-on-Write Fork for xv6

---
# Implement copy-on write (hard)

在计算机系统中有一种说法，任何系统问题都可以用某种程度的抽象方法来解决。Lazy allocation实验中提供了一个例子。这个实验探索了另一个例子：写时复制分支（copy-on write fork）

核心思想：
	**Copy-on-Write (COW) fork 的目标**：通过推迟物理内存页面的分配和复制，仅在子进程实际需要修改内存时才进行物理页面分配，从而提高 fork() 的效率并减少不必要的内存开销。

A. 
- 在 fork() 调用时，不立即为子进程复制父进程的物理内存页面，而是让子进程的页表项（PTE）与父进程共享相同的物理页面。
- 子进程的 PTE 被标记为只读并添加 COW 属性（在 riscv.h 中定义 COW 标志，例如使用 PTE 的保留位），将父进程的 PTE 也标记为只读并添加 COW 属性
- 严谨思考（不影响测试）：父子标记谁先谁后，见文末。

B.
- 当触发页面错误（scause == 13 表示 Load Page Fault，scause == 15 表示 Store/AMO Page Fault）时：
    1. **验证虚拟地址有效性**：
        - 检查虚拟地址是否超出进程的内存大小（sz）、是否大于 MAXVA（最大虚拟地址）、是否位于栈的保护页面（Guard Page）。
        - 若无效，直接终止进程（返回错误）。
    2. **获取物理地址**：
        - 使用 walk() 函数从页表中获取对应的 PTE 和物理地址。
        - **在调用 walk() 前必须确保虚拟地址有效，否则会导致内核 panic（如踩坑点所述）**。
    3. **检查 COW 属性**：
        - 如果 PTE 标记为 COW：
            - 分配一个新的物理页面（kalloc()）。
            - 将原物理页面内容复制到新页面。
            - 更新当前进程的 PTE，指向新物理页面，设置读写权限并标记为有效（PTE_V）。
            - **成功**：减少原物理页面的引用计数，若计数为 0 则释放（kfree()）。
            - **失败**：释放新页面，恢复原 PTE 状态，终止进程。


C. 
- 在 copyout() 中提前处理 COW 页面，因为 **copyout 在内核态调用，触发页面错误会导致导致 kerneltrap**。
  （处理逻辑和 usertrap 中的相同）


D. 
- 在原始 XV6 中，除 trampoline 页面外，每个物理页面只属于一个用户进程。
    - trampoline 页面在用户和内核页表中共享，用于态切换，且永不释放，因此多进程引用无影响。
- COW 引入后，物理页面可能被多个进程的页表共享，且这些页面最终需要释放，这带来了新的挑战：
    1. **问题 a**：若父进程释放了共享页面，而子进程仍在使用，会导致页面错误。
        - **解决**：引入全局引用计数数组，跟踪每个物理页面的引用数；仅当引用计数为 0 时释放页面。
    2. **问题 b**：多 CPU 并发操作引用计数可能导致竞争条件（如 P1 fork、P2 退出同时修改计数）。
        - **解决**：使用自旋锁（spinlock）保护引用计数操作，确保原子性。
- **实现**：模仿 kalloc.c 中的 kmem 结构体，定义一个 ref_count 结构体，包含全局引用计数数组和自旋锁。

E. 
- 在 kalloc.c 中添加关于引用计数的逻辑：
	- kinit：初始化自旋锁。
	- kalloc：分配页面时引用计数设为 1。
	- kfree：减少引用计数，为 0 时释放。
	- freerange：？（暂且不管）


(1) 在 kernel/riscv.h 中使用 PTE 的保留位（例如 PTE_RSW）定义 PTE_F，标记 COW 页面。

```
// 记录应用了COW策略后fork的页面
#define PTE_F (1L << 8)
```

(2) 在 vm.c 修改 uvmcopy（fork 的内存地址复制的逻辑实现在 uvmcopy）
- 为子进程映射父进程页面，标记为只读和 PTE_F。
- 仅在子进程映射成功后，将父进程 PTE 修改为只读和 PTE_F。
```
// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *oldpte;
  uint64 pa, i;
  uint flags;
  //char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((oldpte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*oldpte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*oldpte);
    flags = PTE_FLAGS(*oldpte); 
    
    //添加
    if(mappages(new, i, PGSIZE, pa, (flags & ~PTE_W) | PTE_F) != 0){
        uvmunmap(new, 0, i / PGSIZE, 1);
        return -1;
    }

    *oldpte = (*oldpte & ~PTE_W) | PTE_F; //修改父进程的标志位
    acquire(&ref.lock);
    ++ref.cnt[(uint64)pa / PGSIZE];
    release(&ref.lock);

    /*
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
    */
  }
  return 0;
 /*
 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
  */
}
```


**(3)(4) 步中的注释的？？？代码是否添加在文末解释**
(3) 在 trap.c 中的 usertrap 添加页面错误的处理逻辑（其中处理 cow 逻辑）
处理页面错误（scause == 13 或 15），若为 COW 页面，分配新页面并更新映射
```
...
 // ok
  } else if(r_scause() == 13 || r_scause() == 15){
      uint64 fault_va = r_stval();
      uint64 va = PGROUNDDOWN(fault_va);
      if(fault_va >= p->sz || fault_va >= MAXVA || (fault_va < PGROUNDDOWN(p->trapframe->sp) && fault_va >= (PGROUNDDOWN(p->trapframe->sp) - PGSIZE))){
          p->killed = 1;
      } else{
          pte_t *oldpte = walk(p->pagetable, va, 0);
          uint64 oldpa = PTE2PA((uint64)*oldpte);
          uint64 flags = PTE_FLAGS(*oldpte);
          if(oldpte == 0){
              p->killed = 1;
          } else if(*oldpte & PTE_F){
              char* newpa = kalloc();
              if(newpa == 0){
                  p->killed = 1;
              } else{
                  memmove(newpa, (char*)oldpa, PGSIZE);

                  *oldpte &= ~PTE_V;

                  if(mappages(p->pagetable, va, PGSIZE, (uint64)newpa, (flags | PTE_W) & ~PTE_F) != 0){
                      *oldpte |= PTE_V; //？？？
                      kfree(newpa);
                      p->killed = 1;
                  } else{
                      *oldpte |= PTE_V; //？？？
                      kfree((char*)PGROUNDDOWN(oldpa));
                  }
              }
          }
      }
  } 
```


(4) 在 vm.c 中的 copyout 添加页面错误的处理逻辑（其中处理 cow 逻辑）
检查目标页面是否为 COW，若是则预处理。
```
// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
    uint64 n, va0, pa0;
  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    if(dstva >= MAXVA)
        return -1;

    pa0 = walkaddr(pagetable, va0);
    pte_t *pte = walk(pagetable, va0, 0);
    if(pa0 == 0)
        return -1;

    //添加
    uint64 flags = PTE_FLAGS((uint64)*pte);
    if(*pte & PTE_F){
        char* newpa = kalloc();
        if(newpa == 0){
            return -1;
        }
        memmove(newpa, (char*)pa0, PGSIZE);

        *pte &= ~PTE_V;
        if(mappages(pagetable, va0, PGSIZE, (uint64)newpa, (flags | PTE_W ) & ~PTE_F) != 0){
            *pte |= PTE_V; //？？？？
            kfree(newpa);
            return -1;
        }
        *pte |= PTE_V; //？？？？
        kfree((char*)PGROUNDDOWN(pa0));
        pa0 = (uint64)newpa;
    }
    if(pa0 == 0)
        return -1;
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}
```

(5) kalloc.c
**所有对引用计数的操作都需要上锁来保证原子性**
```
// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "kalloc.h" //添加

extern struct ref_count ref; //添加
struct kmem kmem;//添加

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&ref.lock, "ref"); //添加
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    acquire(&ref.lock);
    ref.cnt[(uint64)p / PGSIZE] = 1;
    release(&ref.lock); //添加
    kfree(p);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (uint64)pa < KERNBASE || (uint64)pa >= PHYSTOP)
    panic("kfree");

  //添加
  // 只有当引用计数为0了才回收空间
  // 否则只是将引用计数减1
  acquire(&ref.lock);
  if(--ref.cnt[(uint64)pa / PGSIZE] == 0) {
	  release(&ref.lock);
      // Fill with junk to catch dangling refs.
      memset(pa, 1, PGSIZE);

      r = (struct run*)pa;

      acquire(&kmem.lock);
      r->next = kmem.freelist;
      kmem.freelist = r;
      release(&kmem.lock);
  }else{
      release(&ref.lock);
  }
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;

  //添加
  if(r){
    kmem.freelist = r->next;
    acquire(&ref.lock);
    ref.cnt[(uint64)r / PGSIZE] = 1;
    release(&ref.lock);
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
```


(5) kalloc.h
将结构体的定义单独提出来（因为 vm.c 中也用到了 ref 结构体，需要包含 kalloc.h），并加上头文件保护（不加会在编译链接时出现 redefinition 错误）
```
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
```

(6) 在 kalloc.c 和 vm.c 中 include "kalloc.h"，在 kalloc.c 定义全局变量，在 defs.h 中添加 walk 函数声明
kalloc.c
```
#include "kalloc.h" 

struct ref_count ref; // 定义全局变量
```
---

**我的踩坑点：**

- ==**问题1：出现 FAILED -- lost some free pages 25597 (out of 31937)==**
- **定位**：内存泄露的问题，释放内存的逻辑出错，总结了以下原因供参考
	- 对引用计数并发访问没有加锁，或者没加正确；检测 kfree 的语义；检查引用计数的 增加 和减少的部分
我的问题解决：
错误代码：**释放锁后检查引用计数，引入了时间窗口，导致多线程环境下页面管理出错，最终丢失空闲页面**。
```
  acquire(&ref.lock);
  --ref.cnt[(uint64)pa / PGSIZE];
  release(&ref.lock);
  if(ref.cnt[(uint64)pa / PGSIZE] == 0) { 

      // Fill with junk to catch dangling refs.
      memset(pa, 1, PGSIZE);

      r = (struct run*)pa;

      acquire(&kmem.lock);
      r->next = kmem.freelist;
      kmem.freelist = r;
      release(&kmem.lock);
  }
```
正确代码：**递减和检查在临界区内，保证了原子性，避免了竞争条件。**
```
acquire(&ref.lock);
  if(--ref.cnt[(uint64)pa / PGSIZE] == 0) {
	  release(&ref.lock);
      // Fill with junk to catch dangling refs.
      memset(pa, 1, PGSIZE);

      r = (struct run*)pa;

      acquire(&kmem.lock);
      r->next = kmem.freelist;
      kmem.freelist = r;
      release(&kmem.lock);
  }else{
      release(&ref.lock);
  }
```
- **竞争场景**：
    - 假设物理页面 pa 的初始引用计数为 2。
    - **CPU1**：  
        - acquire(&ref.lock) → --ref.cnt[...] = 1 → release(&ref.lock)。  
        - 检查 if(ref.cnt[...] == 0)，此时值为 1，不释放页面。
    - **CPU2**（同时执行）：
        - acquire(&ref.lock) → --ref.cnt[...] = 0 → release(&ref.lock)。  
        - 检查 if(ref.cnt[...] == 0)，为真，执行释放操作（memset 和加入 freelist）。
    - **CPU1 后续**：
        - 如果 CPU1 在 CPU2 释放页面后再次访问 ref.cnt[...] == 0（例如由于调度延迟），可能误认为仍需释放，导致页面被重复加入 freelist。
- **后果**：
    - 页面被错误地多次释放（Double Free），加入空闲链表，导致空闲页面计数异常。
    - XV6 的测试程序（例如 usertests 或 cowtest）检查空闲页面总数，发现实际空闲页面数少于预期（lost some free pages 25597 (out of 31937)）。

- ==**问题2：系统卡在 "xv6 booting"，无 shell 提示符，无错误输出**==
- 了解：
	- XV6 启动流程：
	    1. main.c 调用 kinit() 初始化内存。
	    2. kinit 调用 freerange，构建 freelist。
		3. 后续初始化（如分配内核页表、进程表）依赖 kalloc() 从 freelist 获取页面。
  **定位**：出错原因大概率在 freerange 中
- 发现：freerange 在系统启动时调用（通常由 kinit 触发），用于将物理内存范围（如 pa_start 到 pa_end）加入空闲链表，但在我们的 cow 实现中 kfree 减少引用计数，仅当计数为 0 时释放页面，**全局数组 ref.cnt 默认初始化为 0（C 语言全局变量特性），freerange 调用 kfree(p)，kfree 执行 --ref.cnt[...] = -1，引用计数变为负数**，导致 “系统卡在 "xv6 booting"，无 shell 提示符，无错误输出“
- **原因**：
	- freerange 的目的是初始化空闲内存链表（kmem.freelist）。
	- 如果 ref.cnt[] 从 0 减到 -1：
	    - 每次 kfree(p) 都不会将页面加入 freelist（因为 -1 != 0）。
	    - 循环结束后，kmem.freelist 仍为空。
    - freelist 为空，kalloc() 返回 NULL 或陷入死循环（视实现而定）。
    - 内核无法分配内存给关键结构（如第一个用户进程的页表），导致启动流程在 "xv6 booting" 后卡住。
- **解决**：
	- 在 freerange 中将 ref.cnt 初始化为 1。
	- 修改 kfree 的释放条件，从 == 0 改为 <= 0。


- ==**问题3：（3）（4）步中注释???代码的问题，出于严谨性思考mappages 后是否需要恢复有效位**==
- **经过测试无论是否添加注释代码都可以通过但我认为这是测试的小 bug，==严谨的应该为无论 mappages 是否成功都将有效位恢复为 1**==
- **原因**：
	- **成功时**：mappages 已设置 PTE_V，但显式恢复无害（只是冗余）。
	- **失败时**：恢复 PTE_V 确保 va0 仍映射 pa0，避免意外无效状态。
	
我的理解：“在进行cow 页面的处理，需要用新物理页面的映射覆盖旧物理页面的映射，旧物理页面的映射显然已经存在也就是有效位为1，为了不出现 remap 的错误，首先将有效位置为 0 使得可以进行 mappages，如果 mappages 成功即新的映射添加成功，应该将有效位置为 1，如果映射失败，说明新的物理页面的映射没有成功覆盖旧的物理页面的映射，即映射的仍为旧的物理页面，旧的物理页面的映射原来是有效的所以需要将有效位恢复为 1， 所以应该无论mappages是否成功，都应该将有效位恢复为 1”




- ==**问题**4：严谨思考 uvmcopy 中父子标记谁先谁后。==
- **若先修改父进程 PTE 为只读和 COW，子进程映射失败会导致父进程页面状态错误**
- **后果**：父进程访问有效页面时触发页面错误。
- **解决**：在 uvmcopy 中，先为子进程建立映射，成功后再修改父进程 PTE。
	
- ==**问题**5：出现 test copyout: panic: walk==
- **原因**：没有在调用 walk 之前检查虚拟地址的有效性
- **解决**：在调用 walk() 前检查是否超出最大虚拟地址（va >= MAXVA）。

- ==问题6：usertests 卡在 test stacktest: 无报错==
- 定位：栈溢出、页面错误处理
- **原因**：没有检查无效区域——栈的保护页
- **解决**：在 usertrap 中添加 if 判断条件
```
if(fault_va >= p->sz || fault_va >= MAXVA || (fault_va < PGROUNDDOWN(p->trapframe->sp) && fault_va >= (PGROUNDDOWN(p->trapframe->sp) - PGSIZE)))
```
