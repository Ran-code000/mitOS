# Lab3: page tables

---

# Print a page table (easy)
观察：
```
// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}
```

它首先会遍历整个页表。当遇到有效的页表项pte & PTE_V 并且不在最后一层 pte & (PTE_R|PTE_W|PTE_X)) == 0 的时候，它会递归调用。

*`PTE_V`是用来判断页表项是否有效，而`(pte & (PTE_R|PTE_W|PTE_X)) == 0`则是用来判断是否不在最后一层*。

因为**最后一层页表中页表项中W位，R位，X位起码有一位会被设置为1**。注释里面说所有最后一层的页表项已经被释放了，所以遇到不符合的情况就`panic("freewalk: leaf")`

那么，根据`freewalk`，我们可以写下递归函数。对于每一个有效的页表项都打印其和其子项的内容。如果不是最后一层的页表就继续递归。通过`level`来控制前缀`..`的数量。

```
/**
 * @param pagetable 所要打印的页表
 * @param level 页表的层级
 */
void
_vmprint(pagetable_t pagetable, int level){
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    // PTE_V is a flag for whether the page table is valid
    if(pte & PTE_V){
      for (int j = 0; j < level; j++){
        if (j) printf(" ");
        printf("..");
      }
      uint64 child = PTE2PA(pte);
      printf("%d: pte %p pa %p\n", i, pte, child);
      if((pte & (PTE_R|PTE_W|PTE_X)) == 0){
        // this PTE points to a lower-level page table.
        _vmprint((pagetable_t)child, level + 1);
      }
    }
  }
}

/**
 * @brief vmprint 打印页表
 * @param pagetable 所要打印的页表
 */
void
vmprint(pagetable_t pagetable){
  printf("page table %p\n", pagetable);
  _vmprint(pagetable, 1);
}

```

PTE2PA是什么？
	宏，从 PTE 提取物理地址，计算方式为 (pte >> 10) << 12。

为什么计算方式为 (pte >> 10) << 12？
		在 RISC-V Sv39 模式下，页表是三级结构（根页表、第 1 级、第 2 级），每个页表包含 512 个页表项（PTE）。每个 PTE 是 64 位，结构如下（参考 kernel/riscv.h）：
		
- **低 10 位**：标志位：
    - PTE_V (bit 0): 表示 PTE 是否有效。
    - PTE_R (bit 1): 可读（Readable）。  
    - PTE_W (bit 2): 可写（Writable）。  
    - PTE_X (bit 3): 可执行（Executable）。  
    - PTE_U (bit 4): 用户态可访问。
    - 其他位（如 PTE_G, PTE_A, PTE_D）用于全局、访问和脏位。
- **第 10-53 位**：44 位物理页号（PPN）。
- **高 10 位**：保留。  

为什么最后一层页表中页表项中W位，R位，X位起码有一位会被设置为1？

PTE 的作用有两种：

1. **指向下一级页表**
2. **映射物理页面**

#### 1. **页表层级的逻辑**

- RISC-V Sv39 的三级页表设计：
    - 根页表（第 0 级）：PTE 指向第 1 级页表。
    - 第 1 级页表：PTE 指向第 2 级页表。
    - 第 2 级页表（最后一级）：PTE 直接映射物理页面。
- 非最后一级的 PTE（指向下一级页表）：
    - 只需 PTE_V = 1，表示有效，其他权限位（PTE_R、PTE_W、PTE_X）为 0，因为它不直接映射数据，只是“导航”。
- 最后一级的 PTE：
    - 必须映射实际的物理页面，用于存储数据、代码或栈。
    - 物理页面总有某种用途，因此需要至少一个权限位：
        - PTE_R: 数据页面（如全局变量）。
        - PTE_W: 可写数据页面（如栈）。
        - PTE_X: 代码页面（如程序指令）。
    - 如果 PTE_R | PTE_W | PTE_X 均为 0，则 PTE 没有定义用途，逻辑上不合理。

---

# A kernel page table per process (hard)

！！！让每个进程都有自己的内核页表，这样在内核中执行时使用它自己的内核页表的副本，每个进程独立的内核页表避免了全局页表的修改冲突。

**(1)** 首先给 kernel/proc.h_里面的`struct proc`加上内核页表的字段。

```
// Per-process state
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;        // Process state
  struct proc *parent;         // Parent process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;               // Virtual address of kernel stack
  uint64 sz;                   // Size of process memory (bytes)
  pagetable_t pagetable;       // User page table 
  
  //添加
  pagetable_t kernelpt;        // kernel page table
  
  struct trapframe *trapframe; // data page for trampoline.S
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)

};
```


**(2)** 在`vm.c`中添加新的方法`proc_kpt_init`，该方法用于在`allocproc` 中初始化进程的内核页表。这个函数还需要一个辅助函数`proc_kvmmap`，该函数和`kvmmap`方法几乎一致，不同的是`kvmmap`是对Xv6的内核页表进行映射，而 `proc_kvmmap`将用于进程的内核页表进行映射。

```

// add a mapping to the user's kernel page table.
void
proc_kvmmap(pagetable_t pagetable, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(pagetable, va, sz, pa, perm) != 0)
    panic("uvmmap");
}

// Create a kernel page table for the process
pagetable_t
proc_kpt_init(){
  pagetable_t kernelpt = uvmcreate();
  proc_kvmmap(kernelpt, UART0, UART0, PGSIZE, PTE_R | PTE_W);
  proc_kvmmap(kernelpt, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);
  proc_kvmmap(kernelpt, CLINT, CLINT, 0x10000, PTE_R | PTE_W);
  proc_kvmmap(kernelpt, PLIC, PLIC, 0x400000, PTE_R | PTE_W);
  proc_kvmmap(kernelpt, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);
  proc_kvmmap(kernelpt, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);
  proc_kvmmap(kernelpt, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
  return kernelpt;
}
```


**(3)** 根据提示，为了确保每一个进程的内核页表都关于该进程的内核栈有一个映射。我们需要将`procinit`方法中相关的代码迁移到`allocproc`方法中。

```
// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }
  
  //添加：
  // An empty kernel page table.
  p->kernelpt = proc_kpt_init();
  if(p->kernelpt == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  //添加：
  char *pa = kalloc();
  if(pa == 0)
       panic("kalloc");
  uint64 va = KSTACK((int) (p - proc));
  proc_kvmmap(p->kernelpt, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  p->kstack = va;

  return p;
}
```

**(4)** 我们需要修改`scheduler()`来加载进程的内核页表到 SATP 寄存器。

提示里面请求阅读`kvminithart()`
```
// Switch h/w page table register to the kernel's page table,
// and enable paging.
void
kvminithart()
{
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
}
```

`kvminithart`是用于原先的内核页表
需要为进程的内核页表添加一个新方法`proc_inithart`

添加辅助函数 `proc_inithart()`

```
// Store kernel page table to SATP register
void
proc_inithart(pagetable_t kpt){
  w_satp(MAKE_SATP(kpt));
  sfence_vma();
}
```


然后在`scheduler()`内调用即可，但在结束的时候，需要切换回原先的`kernel_pagetable`。直接调用调用上面的`kvminithart()`就能把Xv6的内核页表加载回去。

`scheduler()`：
```
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();

  c->proc = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();

    int found = 0;
    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;

        //添加：
        proc_inithart(p->kernelpt);
		
		//switch context
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
		
		//添加：
        kvminithart();

        found = 1;
      }
      release(&p->lock);
    }
#if !defined (LAB_FS)
    if(found == 0) {
      intr_on();
      asm volatile("wfi");
    }
#else
    ;
#endif
  }
}
```


**(5)** 在`freeproc`中释放一个进程的内核页表。首先释放页表内的内核栈，调用`uvmunmap`可以解除映射，最后的一个参数（`do_free`）为一的时候，会释放实际内存。

```
// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  //添加
  if(p->kernelpt)
  {
	  // free the kernel stack int the RAM
	  uvmunmap(p->kernelpt, p->kstack, 1, 1);
	  p->kstack = 0;

	  proc_freekernelpt(p->pagetable);
  }
  p->pagetable = 0;
  p->kernelpt = 0; //添加
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}
```

然后释放进程的内核页表，类比方法 proc_freepagetable 即释放页表也释放物理内存

```
// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

```


题目要求只释放进程的内核页表不释放物理内存，在 proc.c 添加方法`proc_freekernelpt`

```
// Free a process's kernel page table, but don't free the
// physical memory it refers to.
void
proc_freekernelpt(pagetable_t kernelpt)
{
  proc_kvmfree(kernelpt);
}
```

在 vm.c 中类比 uvmfree 方法实现 proc_kvmfree

```
// Free user memory pages,
// then free page-table pages.
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable);
}

// Only free user's kernel page-table pages.
void
proc_kvmfree(pagetable_t kernelpt)
{
    proc_kptfreewalk(kernelpt);
}
```

在 vm.c 中类比 freewalk 方法实现辅助函数 proc_kptfreewalk

```
// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}

void
proc_kptfreewalk(pagetable_t kernelpt)
{
  // similar to the freewalk method
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = kernelpt[i];
    if(pte & PTE_V){
      kernelpt[i] = 0;
      if ((pte & (PTE_R|PTE_W|PTE_X)) == 0){
        uint64 child = PTE2PA(pte);
        proc_kptfreewalk((pagetable_t)child);
      }
    }
  }
  kfree((void*)kernelpt);
}
```

**(6)** 将需要的函数定义添加到 `kernel/defs.h` 中
```
...
// proc.c
int             cpuid(void);
void            exit(int);
int             fork(void);
int             growproc(int);
pagetable_t     proc_pagetable(struct proc *);
void            proc_freepagetable(pagetable_t, uint64);
void            proc_freekernelpt(pagetable_t);//添加
int             kill(int);
struct cpu*     mycpu(void);
struct cpu*     getmycpu(void);
struct proc*    myproc();
void            procinit(void);
void            scheduler(void) __attribute__((noreturn));
void            sched(void);
void            setproc(struct proc*);
void            sleep(void*, struct spinlock*);
void            userinit(void);
int             wait(uint64);
void            wakeup(void*);
void            yield(void);
int             either_copyout(int user_dst, uint64 dst, void *src, uint64 len);
int             either_copyin(void *dst, int user_src, uint64 src, uint64 len);
void            procdump(void);

...

// vm.c
void            kvminit(void);
pagetable_t     proc_kpt_init(void); //添加
void            kvminithart(void);
void            proc_inithart(pagetable_t); //添加
uint64          kvmpa(uint64);
void            kvmmap(uint64, uint64, uint64, int);
void            proc_kvmmap(pagetable_t, uint64, uint64, uint64, int);//添加
int             mappages(pagetable_t, uint64, uint64, uint64, int);
pagetable_t     uvmcreate(void);
void            uvminit(pagetable_t, uchar *, uint);
uint64          uvmalloc(pagetable_t, uint64, uint64);
uint64          uvmdealloc(pagetable_t, uint64, uint64);
#ifdef SOL_COW
#else
int             uvmcopy(pagetable_t, pagetable_t, uint64);
#endif
void            uvmfree(pagetable_t, uint64);
void            proc_kvmfree(pagetable_t); //添加
void            uvmunmap(pagetable_t, uint64, uint64, int);
void            uvmclear(pagetable_t, uint64);
uint64          walkaddr(pagetable_t, uint64);
int             copyout(pagetable_t, uint64, char *, uint64);
int             copyin(pagetable_t, char *, uint64, uint64);
int             copyinstr(pagetable_t, char *, uint64, uint64);
void            vmprint(pagetable_t);
```

**(7)** 修改`vm.c`中的`kvmpa`，将原先的`kernel_pagetable`改成`myproc()->kernelpt`，使用进程的内核页表

```
// translate a kernel virtual address to
// a physical address. only needed for
// addresses on the stack.
// assumes va is page aligned.
uint64
kvmpa(uint64 va)
{
  uint64 off = va % PGSIZE;
  pte_t *pte;
  uint64 pa;

  pte = walk(myproc()->kernelpt, va, 0); //修改
  if(pte == 0)
    panic("kvmpa");
  if((*pte & PTE_V) == 0)
    panic("kvmpa");
  pa = PTE2PA(*pte);
  return pa+off;
}
```

**(8)** 测试一下我们的代码，先跑起`qemu`，然后跑一下`usertests`。这部分耗时会比较长。


第一次遇到 ==panic: remap== 原因是内核栈映射的时候把proc_kvmmap误写成了kvmmap, 而且内核映射的时候没有正确用映射，索引应该为 p - proc 而不是全部映射到 0，会导致重复映射

第二次报错遇到 ==panic: virtio_disk_intr status==，原因是在scheduler()中错误的在切换上下文(swtch)之后才加载进程的内核页表到核心的`satp`寄存器，应该在之前

---

==疑问：为什么有 uvmcreate 但没有 kvmcreate？==

```
// create an empty user page table.
// returns 0 if out of memory.
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}
```

```
/*
 * create a direct-map page table for the kernel.
 */
void
kvminit()
{
  kernel_pagetable = (pagetable_t) kalloc();
  memset(kernel_pagetable, 0, PGSIZE);
  ...
  //通过 kvmmap() 添加内核所需的映射
}

// Create a kernel page table for the process
pagetable_t
proc_kpt_init(){
  pagetable_t kernelpt = uvmcreate();
  ...
  //通过 proc_kvmmap() 添加用户内核页表所需的映射
  return kernelpt;
}

```

- **用户页表**：每个进程有独立的页表（pagetable_t pagetable），用于映射用户态的虚拟地址到物理地址。
- **内核页表**：全局的 kernel_pagetable 用于映射内核态的虚拟地址（如内核代码、硬件寄存器等），所有进程共享。

- **kalloc()**：  
    - 定义在 kernel/kalloc.c 中。
    - 功能：分配一页物理内存（PGSIZE，通常 4KB），返回指向该内存的指针。
    - 用法：通常用于分配原始内存块，不涉及页表初始化。
- **uvmcreate()**：  
    - 定义在 kernel/vm.c 中。
    - 功能：创建一个空的页表，用于用户进程，返回 pagetable_t。

- 用户页表需要动态创建，uvmcreate() 封装了分配和初始化。
- 内核页表是全局单一的，kalloc() + memset() 已足够完成内核页表的分配和清零，映射逻辑在 kvminit() 中完成。

==解释：==
```
void kvminithart() {
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
}
```

- **使用场景**：
    - 在系统启动时（如 start() 或 main()）调用，启用内核的虚拟内存。
    - 也在 scheduler() 中调用，用于在进程运行结束后恢复全局内核页表
-  在 RISC-V 架构中，SATP 寄存器控制虚拟内存的启用和页表位置。
    - 格式（Sv39 模式）：`[63:60] Mode | [59:44] ASID | [43:0] PPN`  
        - Mode：分页模式（8 表示 Sv39，0 表示禁用）。
        - ASID：地址空间标识符（未在 xv6 中使用，通常为 0）。
        - PPN：页表根节点的物理页面号（Physical Page Number）。```
- **w_satp()**：  
    - 定义在 kernel/riscv.h 中，用于写入 SATP 寄存器。
```
static inline void w_satp(uint64 x) {
  asm volatile("csrw satp, %0" : : "r" (x));
}
```
  
- **MAKE_SATP()**：  
    - 一个宏，将页表地址转换为 SATP 寄存器的值。
```
#define MAKE_SATP(pagetable) ((uint64)8 << 60 | ((uint64)(pagetable) >> 12))
```

- **sfence_vma()**：  
    - 刷新 TLB（Translation Lookaside Buffer，翻译后备缓冲器），确保页表切换生效。
```
static inline void sfence_vma() {
  asm volatile("sfence.vma zero, zero");
}
```


==解释：==
```
asm volatile("csrw satp, %0" : : "r" (x));
```
GCC（GNU Compiler Collection）提供的**内联汇编（Inline Assembly）** 语法，用于在 C 代码中直接嵌入汇编指令。xv6 使用这种方式与 RISC-V 硬件交互，例如设置控制状态寄存器（CSR）。

- **asm**：  
    - 关键字，表示这是一个内联汇编块。
- **volatile**：  
    - 修饰符，告诉编译器不要优化或重排这条汇编指令，确保它按原样执行。
    - 因为这条指令有副作用（修改硬件状态），必须严格执行。
- **"csrw satp, %0"**：  
    - 汇编指令字符串，描述要执行的 RISC-V 指令。
    - csrw：Control and Status Register Write，写入 CSR 寄存器。
    - satp：目标寄存器，Supervisor Address Translation and Protection 寄存器。
    - %0：占位符，表示第一个输入操作数（这里是变量 x）。
- **: :** ： 
    - 分隔符，分隔输出操作数、输入操作数和受影响的寄存器。
    - 第一个 : 后为空，表示没有输出操作数（这条指令只写寄存器，不返回结果）。
    - 第二个 : 后是输入操作数。
- **"r" (x)**：  
    - 输入操作数约束。
    - "r"：告诉编译器将变量 x 放入一个通用寄存器（register）。
    - (x)：C 变量 x，其值会被传递给 %0。

 **翻译成汇编**

这条 C 代码生成的汇编指令是：
```
csrw satp, xN
```

- xN 是编译器分配的寄存器（如 x10），存储变量 x 的值。
- 效果：将 x 的值写入 SATP 寄存器。
#### **功能**

- 在 RISC-V 中，SATP 寄存器控制虚拟内存的启用和页表位置。
- 这条指令将 x（通常是页表地址经过 MAKE_SATP 处理后的值）写入 SATP，从而切换页表并启用分页。

- kernel/riscv.h 是 xv6 的一个头文件，定义了与 RISC-V 架构相关的常量和函数
- **kernel/riscv.h 的典型使用者**：
    1. **kernel/vm.c**：  
        - 用于页表管理，如 kvminithart()、proc_inithart() 调用 w_satp()。
    2. **kernel/proc.c**：  
        - 进程管理中可能用到线程指针（tp）或状态寄存器。
    3. **kernel/trap.c**：  
        - 中断和异常处理需要读写 CSR（如 sstatus、sepc）。
    4. **kernel/main.c**：  
        - 系统启动时初始化硬件状态。
    5. **kernel/swtch.S**（间接相关）：
        - 汇编代码中直接使用 CSR，但 C 代码通过 riscv.h 与之配合。


==疑问：vm.c和proc.c是干什么的，有什么区别==

kernel/vm.c 和 kernel/proc.c 是两个核心源文件，分别负责虚拟内存管理和进程管理

 ***1. kernel/vm.c* 
 
- **vm.c 全称 "virtual memory"（虚拟内存）**，负责管理 xv6 的虚拟内存系统。
- 它实现了页表的管理、虚拟地址到物理地址的映射，以及与 RISC-V 硬件的虚拟内存机制的交互。

**主要功能**

1. **内核页表初始化**：
    - kvminit()：创建并初始化全局内核页表（kernel_pagetable），映射内核代码、数据和硬件设备地址。
    - kvminithart()：将内核页表加载到 SATP 寄存器，启用分页。
2. **页表操作**：
    - mappages()：将虚拟地址映射到物理地址，设置权限。
    - walk()：遍历页表，查找或创建页表条目（PTE）。
    - kvmmap()：封装 mappages()，用于内核页表映射。
    - proc_kvmmap()（lab3实现）：为进程的内核页表添加映射。
3. **用户页表管理**：
    - uvmcreate()：创建空的页表，通常用于用户进程。
    - uvmunmap()：解除虚拟地址的映射，可选择释放物理内存。
    - copyout()/copyin()：在内核和用户空间之间复制数据。
4. **内存释放**：
    - freewalk()：递归释放页表占用的内存。
5. **调试工具**：
    - vmprint()（实验中可能实现）：打印页表内容。

**关键数据结构**

- **pagetable_t**：页表类型，通常是一个指向页表根节点的指针。
- **pte_t**：页表条目类型，包含物理地址和权限位。

 **使用场景**

- 系统启动时初始化内核虚拟内存。
- 为用户进程创建和管理页表。
- 处理虚拟地址翻译和内存访问。



 **2. kernel/proc.c**

 **作用**

- **proc.c 全称 "process"（进程）**，负责管理 xv6 的进程生命周期和状态。
- 它实现了进程的创建、调度、切换和销毁。

 **主要功能**

1. **进程创建**：
    - allocproc()：分配并初始化一个进程结构（struct proc）。
    - proc_pagetable()：为进程创建用户页表。
    - proc_kpt_init()（lab3实现）：为进程创建内核页表。
2. **进程调度**：
    - scheduler()：选择并运行就绪的进程。
    - swtch()（调用 kernel/swtch.S）：切换进程上下文。
3. **进程状态管理**：
    - fork()：创建子进程。
    - exit()：终止当前进程。
    - wait()：等待子进程结束。
    - kill()：发送信号杀死进程。
4. **进程清理**：
    - freeproc()：释放进程资源。
    - proc_freekernelpt()（lab3实现）：释放进程的内核页表。
5. **进程信息**：
    - myproc()：返回当前运行的进程。
    - cpu->proc：跟踪每个 CPU 当前进程。

**关键数据结构**

- **struct cpu**：表示每个 CPU 的状态。
```
// Per-CPU state.
struct cpu {
  struct proc *proc;          // The process running on this cpu, or null.
  struct context context;     // swtch() here to enter scheduler().
  int noff;                   // Depth of push_off() nesting.
  int intena;                 // Were interrupts enabled before push_off()?
};
```

- **struct proc**：  
```
// Per-process state
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;        // Process state
  struct proc *parent;         // Parent process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;               // Virtual address of kernel stack
  uint64 sz;                   // Size of process memory (bytes)
  pagetable_t pagetable;       // User page table
  pagetable_t kernelpt;        // kernel page table
  struct trapframe *trapframe; // data page for trampoline.S
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)

};
```


 **使用场景**
- 创建和管理用户进程。
- 在多核环境下调度进程。
- 处理系统调用和进程退出。

***3. vm.c 和 proc.c 的区别***

| **方面**   | **vm.c**                                  | **proc.c**                                          |
| -------- | ----------------------------------------- | --------------------------------------------------- |
| **主要职责** | 虚拟内存管理                                    | 进程管理                                                |
| **核心功能** | 创建和操作页表，映射虚拟地址                            | 创建、调度和销毁进程                                          |
| **数据结构** | pagetable_t, pte_t  <br>pagetable_t、pte_t | struct proc, struct cpu  <br>struct proc、struct cpu |
| **硬件交互** | 与 SATP、TLB 等虚拟内存硬件直接交互                    | 与上下文切换、中断间接相关                                       |
| **调用时机** | 系统启动、页表初始化、内存访问                           | 进程创建、调度、退出                                          |
| **依赖关系** | 提供内存支持给进程                                 | 使用 vm.c 的页表功能为进程分配内存                                |
| **典型函数** | kvminit(), mappages(), uvmcreate()  <br>  | allocproc(), scheduler(), fork() ,procinit()        |
 **详细区别**

1. **功能范围**：
    - vm.c 关注底层内存管理，处理虚拟地址到物理地址的映射。
    - proc.c 关注进程的生命周期和状态管理，依赖内存管理来运行。
2. **抽象层次**：
    - vm.c 是较低层次，直接与 RISC-V 硬件交互（如设置 SATP）。
    - proc.c 是较高层次，管理进程逻辑，调用 vm.c 的函数。
3. **依赖关系**：
    - proc.c 需要 vm.c 提供页表支持
	    - proc.c 的proc_pagetable() 调用 uvmcreate()；
	    - proc.c 的 scheduler() 调用 vm.c 的 proc_inithart() 和 kvminithart()。
4. **执行上下文**：
    - vm.c 的函数（如 kvminit()）通常在启动时或内存操作时调用。
    - proc.c 的函数（如 scheduler()）在运行时持续执行。


==疑问：defs.h中的函数声明为什么不包含 main.c，start.c，sysproc.c，sysfile.c，包含 swtch.S 却不包含 trampoline.S， entry.S 和 kernelvec.S ？而且为什么其他的都是 .h 文件，这些包含 .S 文件？==

 **main.c**
- **内容**：
    - main()：系统启动入口。
    - 其他函数如 mpinit()（多核初始化）。
- **原因**：
    - main() 是入口点，由 start.c 调用，不被其他文件直接调用。
    - 内部函数（如 mpinit()）仅在 main.c 中使用，作用域局部，无需跨文件访问。

**start.c**
- **内容**：
    - start()：RISC-V 低级启动代码，设置栈指针并跳转到 main()。
- **原因**：
    - start() 是汇编和 C 的混合实现，由链接脚本（kernel.ld）指定为入口（_entry 调用）。
- 它不被其他 C 文件调用，仅初始化系统。

entry.S:
```
# qemu -kernel loads the kernel at 0x80000000
# and causes each CPU to jump there.
# kernel.ld causes the following code to
# be placed at 0x80000000.
.section .text
_entry:
        # set up a stack for C.
        # stack0 is declared in start.c,
        # with a 4096-byte stack per CPU.
        # sp = stack0 + (hartid * 4096)
        la sp, stack0
        li a0, 1024*4
        csrr a1, mhartid
        addi a1, a1, 1
        mul a0, a0, a1
        add sp, sp, a0
        # jump to start() in start.c //调用 start.c
        call start
spin:
        j spin
```
    

 **sysproc.c**
- **内容**：
    - 系统调用实现，如 sys_exit()、sys_fork()。
- **原因**：
    - 这些函数通过 syscall.c 的 syscalls 数组间接调用。
    - 它们是内部实现，不直接暴露给其他模块。
- **结论**：
    - 不需要在 defs.h 中声明，syscall.c 已处理分发。

 **sysfile.c**
- **内容**：
    - 文件相关的系统调用，如 sys_open()、sys_read()。
- **原因**：
    - 同 sysproc.c，通过 syscalls[] 调用，作用域限于文件系统实现。

syscall.c 文件部分内容：
```
...
//所有 sysproc.c 和 sysfile.c 中函数的声明
extern uint64 sys_chdir(void);
extern uint64 sys_close(void);
extern uint64 sys_dup(void);
extern uint64 sys_exec(void);
extern uint64 sys_exit(void);
extern uint64 sys_fork(void);
extern uint64 sys_fstat(void);
extern uint64 sys_getpid(void);
extern uint64 sys_kill(void);
extern uint64 sys_link(void);
extern uint64 sys_mkdir(void);
extern uint64 sys_mknod(void);
extern uint64 sys_open(void);
extern uint64 sys_pipe(void);
extern uint64 sys_read(void);
extern uint64 sys_sbrk(void);
extern uint64 sys_sleep(void);
extern uint64 sys_unlink(void);
extern uint64 sys_wait(void);
extern uint64 sys_write(void);
extern uint64 sys_uptime(void);

static uint64 (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
[SYS_pipe]    sys_pipe,
[SYS_read]    sys_read,
[SYS_kill]    sys_kill,
[SYS_exec]    sys_exec,
[SYS_fstat]   sys_fstat,
[SYS_chdir]   sys_chdir,
[SYS_dup]     sys_dup,
[SYS_getpid]  sys_getpid,
[SYS_sbrk]    sys_sbrk,
[SYS_sleep]   sys_sleep,
[SYS_uptime]  sys_uptime,
[SYS_open]    sys_open,
[SYS_write]   sys_write,
[SYS_mknod]   sys_mknod,
[SYS_unlink]  sys_unlink,
[SYS_link]    sys_link,
[SYS_mkdir]   sys_mkdir,
[SYS_close]   sys_close,
};

void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    p->trapframe->a0 = syscalls[num]();
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
```
 
 
**总结**

- 这些文件的函数要么是**入口点**（main()、start()），要么是**内部实现**（系统调用 sysproc.c，sysfile.c），不需跨文件直接调用，因此不在 defs.h 中声明。

 
 **3. 为什么包含 swtch.S 的声明，却不包含 trampoline.S、entry.S 和 kernelvec.S？**

 **swtch.S**
- **内容**：
    - 定义 swtch() 函数，用于进程上下文切换。
```
# Context switch
#
#   void swtch(struct context *old, struct context *new);
#
# Save current registers in old. Load from new.


.globl swtch
swtch:
        sd ra, 0(a0)
        sd sp, 8(a0)
        sd s0, 16(a0)
        sd s1, 24(a0)
        sd s2, 32(a0)
        sd s3, 40(a0)
        sd s4, 48(a0)
        sd s5, 56(a0)
        sd s6, 64(a0)
        sd s7, 72(a0)
        sd s8, 80(a0)
        sd s9, 88(a0)
        sd s10, 96(a0)
        sd s11, 104(a0)

        ld ra, 0(a1)
        ld sp, 8(a1)
        ld s0, 16(a1)
        ld s1, 24(a1)
        ld s2, 32(a1)
        ld s3, 40(a1)
        ld s4, 48(a1)
        ld s5, 56(a1)
        ld s6, 64(a1)
        ld s7, 72(a1)
        ld s8, 80(a1)
        ld s9, 88(a1)
        ld s10, 96(a1)
        ld s11, 104(a1)

        ret

```
    
- **为何包含在 defs.h**：
    - swtch() 被 proc.c（如 scheduler()）和其他地方直接调用。
    - 需要在 defs.h 中声明 C 原型：
```
void swtch(struct context*, struct context*);
```
    
- 是进程管理的核心功能，跨文件使用频繁。

**trampoline.S**
- **内容**：
    - 定义陷阱跳转代码（trampoline），用于用户态和内核态切换。
    - 示例：
```
.globl trampoline
trampoline:
  csrw sscratch, a0
  // ...
```
        
- **为何不包含**：
    - trampoline 是一个内存区域（映射到 TRAMPOLINE 地址），由硬件陷阱机制跳转执行。
    - 不提供 C 可调用的函数接口。
    - 它的符号在 kernel.ld 中用于布局，无需 C 直接调用。


 **entry.S**
- **内容**：
    - 定义系统启动的入口点 _entry，设置栈并跳转到 start()。
    - 示例：
```
 .globl _entry
_entry:
  la sp, stack0
  call start
```

- **为何不包含**：
    - _entry 是链接器指定的启动入口（kernel.ld 中的 ENTRY(_entry)）。
    - 不被 C 代码调用，仅**初始化系统**。
    - 无需 C 接口。


 **kernelvec.S**
- **内容**：
    - 定义**内核陷阱向量（kernelvec）**，处理内核态中断和异常。
    - 示例：
```
.globl kernelvec
kernelvec:
  csrrw a0, sscratch, a0
  // ...
  sret
```

- **为何不包含**：
    - kernelvec 是硬件陷阱的入口点，映射到 KERNELVEC 地址。
    - 由 trap.c 的 kerneltrap() 间接调用，不提供 C 可调用的函数。
    - 无需在 defs.h 中声明。


**小结**
- **swtch.S**：  
    - 提供 C 可调用的函数（swtch()），需声明。
- **trampoline.S、entry.S、kernelvec.S**：  
    - 是硬件跳转的代码块或入口点，不以函数形式暴露给 C，无需声明。


 **4. 为什么包含 .S 文件的声明，而其他是 .h 文件？**

**.S 文件的特殊性**

- **汇编与 C 的交互**：
    - .S 文件（如 swtch.S）用汇编实现，但通过 .globl 定义全局符号，可被 C 调用。
    - 需要在 defs.h 中声明 C 原型，以便其他 .c 文件使用。
- **为何直接在 defs.h 中声明**：
    - swtch.S 是单一功能的实现，声明简单，直接放在 defs.h 避免分散。
    - xv6 设计简洁，不为每个 .S 文件创建单独头文件。

**.h 文件的普遍性**

- **其他声明**：
    - 如 spinlock.h 定义 struct spinlock 和多个函数。
    - 如 proc.h 定义 struct proc 和相关函数。
- **原因**：
    - .h 文件适合组织复杂结构和多个声明，defs.h 只集中关键函数。
- **例子**：
    - void acquire(struct spinlock*) 在 spinlock.h 中，而 void kvminit(void) 在 defs.h 中。  

 
 **总结**
- **不包含 main.c 等**：
    - 这些文件函数是入口或内部实现，无需跨文件调用。
- **包含 swtch.S，不含 trampoline.S 等**：
    - swtch() 是 C 可调用函数，需声明；其他是硬件入口，无 C 接口。
- **混合 .h 和 .S**：
    - .h 用于复杂声明，.S 的关键函数直接在 defs.h 中声明，简化设计

==疑问：只有用了.globl才可以让c代码调用.S的函数吗？==

- .globl 是 GNU 汇编（as）的一个伪指令，表示声明一个全局符号（global symbol）。
-  如果一个 .S 文件中的符号（如函数名）没有用 .globl 声明，它默认是局部符号（local symbol），只在当前汇编文件中可见。
- C 代码通过链接器（ld）调用汇编函数时，需要该函数是全局符号。
- 因此，只有用 .globl 声明的函数，才能在 C 代码中通过函数名直接调用

**例外**：
- 如果 .S 文件中的代码不以函数形式暴露，而是通过其他机制（如跳转表或硬件调用）执行，则无需 .globl。
- 例如，trampoline.S 中的代码通过映射到 TRAMPOLINE 地址被硬件陷阱调用，不需要 C 直接调用，因此不一定用 .globl。


==疑问：.S文件和.s文件有什么区别？==

**区别总结**

| **特性**   | **.S**       | **.s**   |
| -------- | ------------ | -------- |
| **预处理**  | 支持（通过 cpp）   | 不支持      |
| **内容**   | 汇编 + C 宏     | 纯汇编      |
| **编译工具** | gcc（预处理）+ as | 直接 as    |
| **灵活性**  | 高（可使用头文件和宏）  | 低（纯手工编写） |
**xv6 中的使用**
- xv6 统一使用 .S 文件（如 swtch.S、trampoline.S），因为它们需要与 C 代码交互，可能包含宏或头文件（如 param.h 中的常量）。
- 不使用 .s 文件，因为预处理功能在内核开发中更方便。


==疑问：kernel.asm，kernel.ld，kernel.sym，kernelvec.o，kernelvec,S 文件是干什么的？==

- kernel.asm：反汇编，调试用。
    - 通过 riscv64-unknown-elf-objdump -S kernel/kernel > kernel/kernel.asm 生成。
    - .S 表示将目标文件（kernel/kernel）反汇编并混合源代码。
	- 显示内核的可执行文件的汇编代码，便于调试和分析。
	- 包含所有 .c 和 .S 文件编译后的机器指令。
	- 检查生成的代码是否正确，例如验证 swtch() 的实现。
- kernel.ld：链接脚本，布局内核。
- kernel.sym：符号表，调试用。
	- 列出内核中所有全局符号的地址
- kernelvec.o/kernelvec.S：内核陷阱处理。

==疑问：==
==`uvmunmap(p->kernelpt, p->kstack, 1, 1);` 和 `p->kstack = 0;`==
==是否可以移到 if (p->kernelpt) 条件之外==
```
if (p->kernelpt) {
    // free the kernel stack in the RAM
    uvmunmap(p->kernelpt, p->kstack, 1, 1);
    p->kstack = 0;
    proc_freekernelpt(p->pagetable);
}
```

- **当 pagetable = 0**：
    - walk(0, a, 0) 会返回 0（因为页表不存在）。
    - 触发 panic("uvmunmap: walk")。  
        触发 panic（“uvmunmap： walk”）。

**后果**
- 如果 p->kernelpt == 0，调用 uvmunmap(0, p->kstack, 1, 1) 会导致内核 panic()。
- 当前代码通过 if (p->kernelpt) 避免了这种情况。


==XV6 的进程生命周期管理是怎样的？==

完整调用链总结

进程退出并进入调度器

1. **用户态触发**：
    - exit()（用户态） → ecall → uservec → usertrap → syscall → sys_exit → exit  
2. **内核态触发**：
    - usertrap（异常或 killed） → exit  
3. **退出流程**：
    - exit → sched  
4. **调度器**：
    - sched → swtch → scheduler  

父进程回收子进程资源

5. **父进程回收**：
    - wait（用户态） → sys_wait → wait → freeproc
6. **资源释放**：
    - freeproc → kfree(p->trapframe) → proc_freepagetable  
    - proc_freepagetable → uvmunmap(TRAMPOLINE) → uvmunmap(TRAPFRAME) → uvmfree  
    - uvmfree → uvmunmap(0, sz) → freewalk  


**职责分离**：
- uvmunmap 和 uvmfree 负责移除叶子节点映射并释放物理内存。
- freewalk 只负责释放页表结构本身（非叶子节点）。
```
// Free user memory pages,
// then free page-table pages.
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1); //释放物理内存
  freewalk(pagetable);
}

// Only free user's kernel page-table pages.
void
proc_kvmfree(pagetable_t kernelpt)
{
    proc_kptfreewalk(kernelpt);
}
```

==疑问：如何实现 “你需要一种方法来释放页表，而不必释放叶子物理内存页面。“？为什么要实现？但是需要释放进程的内核栈，为什么？==

如何实现？
freewalk:
```
// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0; // 清零有效 PTE
    } else if(pte & PTE_V){ 
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}
```

1. **if ((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0)**：  
    - PTE_V：条目有效。
    - !(PTE_R | PTE_W | PTE_X)：无读/写/执行权限，表示该 PTE 指向下一级页表。
    - child = PTE2PA(pte)：提取下一级页表的物理地址。
    - 递归调用 freewalk(child)。
    - pagetable[i] = 0：清零 PTE，避免重复释放。
    
2. **else if (pte & PTE_V)**：  
    - 如果 PTE 有效且有 PTE_R、PTE_W 或 PTE_X，说明是叶子节点。
    - 触发 panic("freewalk: leaf")，因为要求叶子映射已移除（如 TRAMPOLINE 和 TRAPFRAME）
    - **freewalk 的职责**：
		- 只释放页表本身的物理内存（页面表节点），而不是映射的物理页面。
		- 映射的物理页面（如 TRAMPOLINE 和 TRAPFRAME， 用户代码）应在其他地方释放（如 uvmfree 释放用户代码，proc_freepagetabl 释放  TRAMPOLINE 和 TRAPFRAME 的映射，trapframe 的释放由 freeproc 单独处理 ）
	- **TRAMPOLINE**：  
	    - 虚拟地址：0x3ffffff000。
	    - 映射：指向共享的 trampoline 物理地址，权限为 PTE_R | PTE_X。
	    - 在用户页表中是一个叶子节点。
	- **TRAPFRAME**：  
	    - 虚拟地址：0x3fffffe000。
	    - 映射：指向进程独有的 p->trapframe 物理地址，权限为 PTE_R | PTE_W。
	    - 在用户页表中也是叶子节点。
```
void proc_freepagetable(pagetable_t pagetable, uint64 sz) {
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);  // 移除 TRAMPOLINE 映射
  uvmunmap(pagetable, TRAPFRAME, 1, 0);   // 移除 TRAPFRAME 映射
  uvmfree(pagetable, sz);                 // 释放用户页面和页表
}
```


   3. **kfree((void*)pagetable)**：  
    - 释放当前页表页面。

所以需要预先移除叶子映射，否则 panic()
```
// Free user memory pages,
// then free page-table pages.
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  //移除叶子映射
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  //释放页表
  freewalk(pagetable);
}
```

proc_kptfreewalk：
对叶子节点仅清零 PTE，不释放物理页面，也不 panic()
```
void
proc_kptfreewalk(pagetable_t kernelpt)
{
  // similar to the freewalk method
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = kernelpt[i];
    if(pte & PTE_V){
      kernelpt[i] = 0; // 清零有效 PTE
      if ((pte & (PTE_R|PTE_W|PTE_X)) == 0){
        uint64 child = PTE2PA(pte);
        proc_kptfreewalk((pagetable_t)child);
      }//没有 else
    }
  }
  kfree((void*)kernelpt);
}
```


只释放页表本身（kfree((void*)pagetable)），不释放叶子页面
```
// Only free user's kernel page-table pages.
void
proc_kvmfree(pagetable_t kernelpt)
{
	//无需移除叶子映射
	
    proc_kptfreewalk(kernelpt);
}
```

为什么？
```
pagetable_t proc_kpt_init() {
  pagetable_t kernelpt = uvmcreate();

  //串口设备
  proc_kvmmap(kernelpt, UART0, UART0, PGSIZE, PTE_R | PTE_W);
  //VirtIO 磁盘设备
  proc_kvmmap(kernelpt, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);
  //核心本地中断控制器
  proc_kvmmap(kernelpt, CLINT, CLINT, 0x10000, PTE_R | PTE_W);
  //平台级中断控制器
  proc_kvmmap(kernelpt, PLIC, PLIC, 0x400000, PTE_R | PTE_W);
  //内核代码段
  proc_kvmmap(kernelpt, KERNBASE, KERNBASE, (uint64)etext - KERNBASE, PTE_R | PTE_X);
  //内核数据段和堆
  proc_kvmmap(kernelpt, (uint64)etext, (uint64)etext, PHYSTOP - (uint64)etext, PTE_R | PTE_W);
  //系统调用返回的跳转代码
  proc_kvmmap(kernelpt, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  return kernelpt;
}
```

包含内核全局映射（设备、代码、数据、trampoline），映射硬件地址如 UART0、VIRTIO0 这些地址是固定的，不能用 kfree() 释放多个进程共享这些物理页面（如内核代码、设备内存）
因为如果释放页表时释放了这些页面，会破坏其他进程或内核的功能


要释放进程的内核栈，为什么？
```
if (p->kernelpt) {
  uvmunmap(p->kernelpt, p->kstack, 1, 1); // 释放栈页面
  p->kstack = 0;
  proc_freekernelpt(p->kernelpt);         // 释放页表
}
```

- p->kstack 是进程专用的物理页面，不被其他进程或全局内核共享。
- 每个进程的 p->kstack 地址不同，由 kalloc() 动态分配。
- 当进程退出（exit() 或 kill()）时，其所有私有资源必须释放，包括 p->kstack。
- 如果不释放，物理页面会泄漏，导致内存耗尽。


==疑问：TRAMPOLINE  是进程共享的为什么释放进程的时候要👇，这不是把物理内存也释放了吗，不会影响其他的进程吗，TRAPFRAME 呢==
```
uvmunmap(pagetable, TRAMPOLINE, 1, 0);
uvmunmap(pagetable, TRAPFRAME, 1, 0);
```

**关键在于 do_free = 0**：

- 在 uvmunmap(pagetable, TRAMPOLINE, 1, 0) 和 uvmunmap(pagetable, TRAPFRAME, 1, 0) 中，do_free 参数是 0。
- 这意味着：
    - **不释放物理内存**，仅清除页表条目（*pte = 0）
    - 物理页面（如 trampoline 和 p->trapframe 的物理地址）保持分配状态


对其他进程的影响

- **TRAMPOLINE**：  
    - 无影响。物理内存未释放，其他进程的映射保持有效。
- **TRAPFRAME**：  
    - 无影响。每个进程的 trapframe 是独立的，清理只影响当前进程的页表。trapframe 的释放由 freeproc 单独处理
---

# Simplify `copyin`/`copyinstr`（hard）

上一个任务实现的优化：我们通过让每个进程都有自己的内核页表，这样在内核中执行时可以使用它自己的内核页表的副本，每个进程独立的内核页表避免了全局页表的修改冲突。

现在每个进程都有独立的用户页表和内核页表，但是用户页表和内核页表映射的虚拟空间的位置不同，如果我们需要让内核的`copyin`函数读取用户指针指向的内存。它通过将用户指针转换为内核指针才可以直接解引用的物理地址来实现这一点，本实验的目标是通过将用户空间的映射添加到每个进程的内核页表（将进程的页表复制一份到进程的内核页表就好），使得内核可以直接解引用用户指针，从而简化 copyin 和 copyinstr 的实现。

核心思想：将用户页表的映射同步到内核页表
- userinit()（初始创建）。
- fork()（复制）。
- exec()（替换）。
- sbrk()（调整大小）。

1. userinit()（初始创建）

我们需要在创建第一个进程中初始化页表时就将初始化的用户空间添加到进程内核页表中（在 XV6 中，第一个进程由 userinit() 创建（在 kernel/proc.c 中），它初始化用户空间并设置第一个用户程序（通常是 initcod）

我们知道在 XV6 中，用户空间的虚拟地址是从 0 开始分配，初始化大小为一个页面（PGSIZE）

userinit() 通过 uvminit() 初始化第一个进程的用户空间，大小为一个页面（PGSIZE = 4096 字节），用于加载 initcode。

**补充**：
- 用户空间初始为 PGSIZE，但后续可能通过 sbrk 增长，需动态同步到内核页表。

mappages() 添加映射需要物理地址（pa）来创建页表条目（PTE），格式为 PA2PTE(pa) | flags。而用户空间的物理地址由 kalloc() 分配，在 uvminit() 中生成，但原始版本未返回该地址。

法一：我们可以通过修改 uvminit 函数的返回值（从 void 到 uint64），返回用户空间的物理地址，从而调用 mappages 添加映射。
```
// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;

  // allocate one user page and copy init's instructions
  // and data into it.
  uint64 pa = uvminit(p->pagetable, initcode, sizeof(initcode)); //修改
  p->sz = PGSIZE;

  //添加：
  uint64 va = 0x0;
  if (mappages(p->kernelpt, va, PGSIZE, pa, PTE_R | PTE_W) != 0) {
    panic("userinit: mappages failed");
  }
  
  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;      // user program counter
  p->trapframe->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}
```

我们修改了 uvminit 函数的返回值，需要相应修改 defs.h 中的 uvminit 函数声明
```
// vm.c
...
uint64          uvminit(pagetable_t, uchar *, uint);
...
```

法二：添加辅助函数 u2kvmcopy 
具体操作是：将用户页表的映射同步到内核页表，并调整权限（去掉 PTE_U）
```
void
u2kvmcopy(pagetable_t pagetable, pagetable_t kernelpt, uint64 oldsz, uint64 newsz){
  pte_t *pte_from, *pte_to;
  oldsz = PGROUNDUP(oldsz);
  for (uint64 i = oldsz; i < newsz; i += PGSIZE){
    if((pte_from = walk(pagetable, i, 0)) == 0) //第三个参数0：不创建新条目，仅查找现有条目。
      panic("u2kvmcopy: src pte does not exist");
    if((pte_to = walk(kernelpt, i, 1)) == 0) //第三个参数1：如果 PTE 不存在，创建新的页表节点。
      panic("u2kvmcopy: pte walk failed");
    uint64 pa = PTE2PA(*pte_from);
    uint flags = (PTE_FLAGS(*pte_from)) & (~PTE_U);
    *pte_to = PA2PTE(pa) | flags;
  }
}
```
工作原理
1. **输入**：用户页表、内核页表、复制范围（oldsz 到 newsz）。
2. **过程**：
    - 对齐 oldsz 到页面边界。
    - 遍历每个页面（i 从 oldsz 到 newsz）：
        - 从用户页表获取 PTE（pte_from），提取物理地址（pa）和标志（flags）（调用 u2kvmcopy 前，用户页表应已通过 uvmalloc 或 uvmcopy 设置映射，此处假设映射存在， 否则触发 panic）
        - 在内核页表中创建或定位对应 PTE（pte_to）。
        - 去掉 PTE_U，将映射写入内核页表。内核页表可能尚未映射 i，需要动态分配页表节点（三级页表中的中间层）。
3. **输出**：kernelpt 中新增了从 oldsz 到 newsz 的用户空间映射。
（这里以及后面我们采取法二，不修改原来的 uvminit 函数）



我们思考，我们什么时候需要去将用户空间的映射添加到每个进程的内核页表呢？

- 将用户空间的映射添加到内核页表（p->kernel_pagetable 或 p->kernelpt）的目的是让内核能够直接解引用用户指针（如 copyin_new 和 copyinstr_new 中使用的 srcva）。
- 用户空间的映射需要与实际的用户内存保持一致，因此只有在用户空间的内容（如页面映射）或大小（如 p->sz）发生变化时，才需要更新内核页表。

用户空间的变化包括：
- **大小变化**：通过 sbrk() 增加或减少内存。
- **内容变化**：通过 fork() 复制父进程的内存，或通过 exec() 加载新程序替换原有内存。
（此外，初始创建进程时（例如 userinit()）也需要设置用户空间的映射，作为起点，我们已经完成）

提示：“在内核更改进程的用户映射的每一处 （`fork()`, `exec()`, 和`sbrk()`），都复制一份到进程的内核页表。”

2. `fork()`:
```
int
fork(void){
  ...
  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;
  ...
  // 复制到新进程的内核页表
  u2kvmcopy(np->pagetable, np->kernelpt, 0, np->sz);
  ...
}

```


3. `exec()`：

```
int
exec(char *path, char **argv){
  ...
  sp = sz;
  stackbase = sp - PGSIZE;

  // 添加复制逻辑
  u2kvmcopy(pagetable, p->kernelpt, 0, sz);

  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
  ...
}

```

4. `sbrk()`， 在_kernel/sysproc.c_里面找到`sys_sbrk(void)`，可以知道只有`growproc`是负责将用户内存增加或缩小 n 个字节，因此我们去修改 `growproc` 。为了防止用户进程增长到超过`PLIC`的地址，我们需要给它加个限制。

```
int
growproc(int n)
{
  uint sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    // 加上PLIC限制
    if (PGROUNDUP(sz + n) >= PLIC){
      return -1;
    }
    if((sz = uvmalloc(p->pagetable, sz, sz + n)) == 0) {
      return -1;
    }
    // 复制一份到内核页表
    u2kvmcopy(p->pagetable, p->kernelpt, sz - n, sz);
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

```

完成，我们可以用 `copyin_new ` 和 `copyinstr_new`
 替换掉原有的`copyin()`和`copyinstr()`

实现内核可以直接解引用用户指针

最后将 vmcopyin.c 中的函数的声明添加到 defs.h 中
```
// vmcopyin.c
int             copyin_new(pagetable_t, char *, uint64, uint64);
int             copyinstr_new(pagetable_t, char *, uint64, uint64);
```
``
