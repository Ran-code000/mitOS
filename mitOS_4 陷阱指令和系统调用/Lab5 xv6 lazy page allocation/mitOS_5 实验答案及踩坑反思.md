# Lab5: xv6 lazy page allocation

---
#   Eliminate allocation from sbrk() (easy)

删除对`growproc()`的调用，只需要增加进程的大小

```
uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;

  addr = myproc()->sz;
  // lazy allocation
  myproc()->sz += n;

  return addr;
}

```


---
# Lazy allocation (moderate)

根据提示来做就好，另外6.S081对应的视频课程中对这部分代码做出了很大一部分的解答。

(1) 修改`usertrap()`(**_kernel/trap.c_**)函数，使用`r_scause()`判断是否为页面错误，在页面错误处理的过程中，先判断发生错误的虚拟地址（`r_stval()`读取）是否位于栈空间之上，进程大小（虚拟地址从0开始，进程大小表征了进程的最高虚拟地址）之下，然后分配物理内存并添加映射
```
  uint64 cause = r_scause();
  if(cause == 8) {
    ...
  } else if((which_dev = devintr()) != 0) {
    // ok
  } else if(cause == 13 || cause == 15) {
    // 处理页面错误
    uint64 fault_va = r_stval();  // 产生页面错误的虚拟地址
    char* pa;                     // 分配的物理地址
    if(PGROUNDUP(p->trapframe->sp) - 1 < fault_va && fault_va < p->sz && (pa = kalloc()) != 0) {
        memset(pa, 0, PGSIZE);
        if(mappages(p->pagetable, PGROUNDDOWN(fault_va), PGSIZE, (uint64)pa, PTE_R | PTE_W | PTE_X | PTE_U) != 0) {
          kfree(pa);
          p->killed = 1;
        }
    } else {
      // printf("usertrap(): out of memory!\n");
      p->killed = 1;
    }
  } else {
    ...
  }

```

题目提示：如果某个进程在高于`sbrk()`分配的任何虚拟内存地址上出现页错误，则终止该进程；处理用户栈下面的无效页面上发生的错误

这两个可以统一处理为无效地址，只需在有效地址（PGROUNDUP(p->trapframe->sp) - 1 < fault_va && fault_va < p->sz）内处理即可

(2) 修改`uvmunmap()`(**_kernel/vm.c_**)，之所以修改这部分代码是因为lazy allocation中首先并未实际分配内存，所以当解除映射关系的时候对于这部分内存要略过，而不是使系统崩溃，这部分在课程视频中已经解答。
```
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  ...

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");
    if((*pte & PTE_V) == 0)
      continue;

    ...
  }
}

```

---
# Lazytests and Usertests (moderate)

(1)处理`sbrk()`参数为负数的情况，参考之前`sbrk()`调用的`growproc()`程序，如果为负数，就调用`uvmdealloc()`函数，但需要限制缩减后的内存空间不能小于0

参考的 growproc 实现：
```
// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}
```

sys_sbrk实现
```
uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;

  struct proc* p = myproc();
  addr = p->sz;
  uint64 sz = p->sz;

  if(n > 0) {
    // lazy allocation
    p->sz += n;
  } else if(sz + n > 0) {
    sz = uvmdealloc(p->pagetable, sz, sz + n);
    p->sz = sz;
  } else {
    return -1;
  }
  return addr;
}
```

(2) 正确处理`fork`的内存拷贝：`fork`调用了`uvmcopy`进行内存拷贝，所以修改`uvmcopy`如下
```
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  ...
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      continue;
    if((*pte & PTE_V) == 0)
      continue;
    ...
  }
  ...
}

```

(3)还需要继续修改`uvmunmap`，否则会运行出错，关于为什么要使用两个`continue`，请看本文最下面
```
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  ...

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0)
      continue;
    if((*pte & PTE_V) == 0)
      continue;

    ...
  }
}
```

(4) 处理通过sbrk申请内存后还未实际分配就传给系统调用使用的情况，系统调用的处理会陷入内核，scause寄存器存储的值是8，如果此时传入的地址还未实际分配，会直接导致系统调用失败，不可能回到 usertrap 中判断scause是13或15的代码内处理，原因见文末。

- 系统调用流程：
    
    - 陷入内核 ==>`usertrap`中`r_scause()==8`的分支==>`syscall()`==>回到用户空间

- 页面错误流程：
    
    - 陷入内核 ==>`usertrap`中`r_scause()==13||r_scause()==15`的分支==>分配内存==>回到用户空间

因此就需要找到在何时系统调用会使用这些地址，将地址传入系统调用后，会通过`argaddr`函数(**_kernel/syscall.c_**)从寄存器中读取，因此在这里添加物理内存分配的代码（这里可能会想到另一种思路，但是实现困难且不合理，但值得讨论，后面会细说）

```
int
argaddr(int n, uint64 *ip)
{
  *ip = argraw(n);
  struct proc* p = myproc();

  // 处理向系统调用传入lazy allocation地址的情况
  if(walkaddr(p->pagetable, *ip) == 0) {
    if(PGROUNDUP(p->trapframe->sp) - 1 < *ip && *ip < p->sz) {
      char* pa = kalloc();
      if(pa == 0)
        return -1;
      memset(pa, 0, PGSIZE);

      if(mappages(p->pagetable, PGROUNDDOWN(*ip), PGSIZE, (uint64)pa, PTE_R | PTE_W | PTE_X | PTE_U) != 0) {
        kfree(pa);
        return -1;
      }
    } else {
      return -1;
    }
  }

  return 0;
}

```

---
## 为什么使用两个continue

这里需要解释一下为什么在两个判断中使用了`continue`语句，在课程视频中仅仅添加了第二个`continue`，利用`vmprint`打印出来初始时刻用户进程的页表如下

```
page table 0x0000000087f55000
..0: pte 0x0000000021fd3c01 pa 0x0000000087f4f000
.. ..0: pte 0x0000000021fd4001 pa 0x0000000087f50000
.. .. ..0: pte 0x0000000021fd445f pa 0x0000000087f51000
.. .. ..1: pte 0x0000000021fd4cdf pa 0x0000000087f53000
.. .. ..2: pte 0x0000000021fd900f pa 0x0000000087f64000
.. .. ..3: pte 0x0000000021fd5cdf pa 0x0000000087f57000
..255: pte 0x0000000021fd5001 pa 0x0000000087f54000
.. ..511: pte 0x0000000021fd4801 pa 0x0000000087f52000
.. .. ..510: pte 0x0000000021fd58c7 pa 0x0000000087f56000
.. .. ..511: pte 0x0000000020001c4b pa 0x0000000080007000

```

除去高地址的trapframe和trampoline页面，进程共计映射了4个有效页面，即添加了映射关系的虚拟地址范围是`0x0000~0x3fff`，假如使用`sbrk`又申请了一个页面，由于lazy allocation，页表暂时不会改变，而不经过读写操作后直接释放进程，进程将会调用`uvmunmap`函数，此时将会发生什么呢？

`uvmunmap`首先使用`walk`找到虚拟地址对应的PTE地址，虚拟地址的最后12位表征了偏移量，前面每9位索引一级页表，将`0x4000`的虚拟地址写为二进制（省略前面的无效位）：

```
{000 0000 00}[00 0000 000](0 0000 0100) 0000 0000 0000

```
- `{}`：页目录表索引(level == 2)，为0
- `[]`：二级页表索引(level == 1)，为0
- `()`：三级页表索引(level == 0)，为4

我们来看一下`walk`函数，`walk`返回指定虚拟地址的PTE，但我认为这个程序存在一定的不足。walk函数的代码如下所示

```
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}

```

这段代码中`for`循环执行`level==2`和`level==1`的情况，而对照刚才打印的页表，`level==2`时索引为0的项是存在的，`level==1`时索引为0的项也是存在的，最后执行`return`语句，然而level == 0时索引为4的项却是不存在的，此时`walk`不再检查`PTE_V`标志等信息，而是直接返回，因此即使虚拟地址对应的PTE实际不存在，`walk`函数的返回值也可能不为0！

那么返回的这个地址是什么呢？level为0时

有效索引为0~3，因此索引为4时返回的是最后一个有效PTE后面的一个地址。

**因此我们不能仅靠PTE为0来判断虚拟地址无效，还需要再次检查返回的PTE中是否设置了`PTE_V`标志位，所以需要两个 continue**

---

我开始的异想天开：

为了实现懒分配，我们设计了一种希望被触发的页面错误，这种页面错误发生在用户进程地址空间中，并期望在合法的地址范围内触发。

在题目中要求实现： 处理这种情形：进程从`sbrk()`向系统调用（如`read`或`write`）传递有效地址，但尚未分配该地址的内存。

首先思考这种场景和用户态访问地址无效触发页面错误的场景区别在哪？

用户态访问地址无效触发页面错误：

- 用户态访问地址无效 -> 触发页面错误 -> 进入 usertrap 中进行页面错误处理（r_scause() == 13 || r_scause() == 15） -> 程序重新执行触发错误的指令

题目场景：

- 用户态进行系统调用 -> 陷入内核态执行 usertrap （r_scause() == 8）-> syscall -> 执行系统调用函数 -> 在 copyin 和 copyout 时检查出未映射（PTE 无效），walkaddr 返回 0，copyout 返回 -1 -> 不会触发页面错误，直接返回错误

总结一下就是：用户态触发页面错误进入 usertrap 处理
			 内核态（系统调用后陷入）不等触发页面错误，就检测到无效地址了直接返回错误


我的想法：既然内核态（系统调用后陷入）不等触发页面错误，就检测   到了无效地址直接返回错误，那能不能设计让其触发页面错误（将检查出未映射后返回错误的逻辑注释掉），然后用和 usertrap 中处理用户态页面错误同样的方法在kerneltrap 中处理内核态页面错误

事实证明实现的可行性和合理性不足

可行性：copyout 触发页面错误的地址是物理地址，而不是用户地址（memmove 访问 pa0 + (dst - va0) = 0），理论上可行，但需要正确获取用户地址（dst），而不是物理地址，实现困难；

合理性：内核态（S-mode）页面错误通常是致命的，是不期望出现的（用户态的页面错误是设计的希望出现的）


经过分析，我们发现设计让其内核态触发页面错误然后在 kerneltrap 中处理是不太可行的，那我们就不能让内核态发生页面错误，那就不能注释掉检查出未映射后返回错误的逻辑，我们又不想让系统调用直接返回错误，因为此时我们的地址是有效的只是没有映射，那我们就在**最开始 获得地址的时候**就对未映射的有效地址进行分配（逻辑和 usertrap 中处理懒分配的逻辑一样）

查看write的系统调用函数 sys_write
```
uint64
sys_write(void)
{
  struct file *f;
  int n;
  uint64 p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
    return -1;

  return filewrite(f, p, n);
}
```

得到最开始得到地址的函数为  argaddr

在 argaddr 中添加
```
int
argaddr(int n, uint64 *ip)
{
    *ip = argraw(n);
    struct proc* p = myproc();

    //添加：
    if (walkaddr(p->pagetable, *ip) == 0) {
        if (PGROUNDUP(p->trapframe->sp) - 1 < *ip && *ip < p->sz) {
            char* pa = kalloc();
            if (pa == 0)
                return -1;
            memset(pa, 0, PGSIZE);
            if (mappages(p->pagetable, PGROUNDDOWN(*ip), PGSIZE, (uint64)pa, PTE_R | PTE_W | PTE_X | PTE_U) != 0) {
                kfree(pa);
                return -1;
            }
        } else {
            return -1;
        }
    }
    return 0;
}
```

**效果**：

- 提前分配页面，避免页面错误。
- 启动阶段不会触发 copyout 的页面错误。
---

==现在的懒分配实现粗糙且不严谨，虽然可以通过样例==

==粗糙的原因是：不是所有页面错误都是可以触发懒分配的那种，其他的错误我们直接统一 p->killed =  1 了并没有打印不同错误的日志；==

==不严谨的原因是：对于处理懒分配我们只限定了有效的地址范围，换句话说就是我们对于有效地址的页面错误都按懒分配处理了，这是严谨的说是不对的（比如说权限错误）虽然可以通过测试。==

==以上粗糙和不严谨的修复见同目录下的最后修订==
