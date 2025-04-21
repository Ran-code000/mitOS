# Lab4: traps

---
# RISC-V assembly (easy)

call.c
```
#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int g(int x) {
  return x+3;
}

int f(int x) {
  return g(x);
}

void main(void) {
  printf("%d %d\n", f(8)+1, 13);
  exit(0);
}
```

call.asm
```
user/_call:     file format elf64-littleriscv


Disassembly of section .text:

0000000000000000 <g>:
#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int g(int x) {
   0:   1141                    addi    sp,sp,-16
   2:   e422                    sd      s0,8(sp)
   4:   0800                    addi    s0,sp,16
  return x+3;
}
   6:   250d                    addiw   a0,a0,3
   8:   6422                    ld      s0,8(sp)
   a:   0141                    addi    sp,sp,16
   c:   8082                    ret

000000000000000e <f>:

int f(int x) {
   e:   1141                    addi    sp,sp,-16
  10:   e422                    sd      s0,8(sp)
  12:   0800                    addi    s0,sp,16
  return g(x);
}
  14:   250d                    addiw   a0,a0,3
  16:   6422                    ld      s0,8(sp)
  18:   0141                    addi    sp,sp,16
  1a:   8082                    ret

000000000000001c <main>:

void main(void) {
  1c:   1141                    addi    sp,sp,-16
  1e:   e406                    sd      ra,8(sp)
  20:   e022                    sd      s0,0(sp)
  22:   0800                    addi    s0,sp,16
  printf("%d %d\n", f(8)+1, 13);
  24:   4635                    li      a2,13
  26:   45b1                    li      a1,12
  28:   00000517                auipc   a0,0x0
  2c:   7c050513                addi    a0,a0,1984 # 7e8 <malloc+0xea>
  30:   00000097                auipc   ra,0x0
  34:   610080e7                jalr    1552(ra) # 640 <printf>
  exit(0);
  38:   4501                    li      a0,0
  3a:   00000097                auipc   ra,0x0
  3e:   27e080e7                jalr    638(ra) # 2b8 <exit>
```
answers-traps.txt
```
1. 函数参数保存在寄存器 a0 到 a7 中。在 main 对 printf 的调用 中，13 保存在寄存器 a2。

2. main 中没有显式调用 f 或 g 的汇编指令。编译器将 f 和 g 内联，f(8) + 1 被预计算为 12。

3. printf 函数位于地址 0x630。

4. 在 main 中 printf 的 jalr 之后，寄存器 ra 的值是 0x38，即下一条指令的地址。

5. 运行代码 unsigned int i = 0x00646c72; printf("H%x Wo%s", 57616, &i); 输出是 "H0xe110 World"。
   这是因为 RISC-V 是小端存储，0x00646c72 存储为 72 6c 64 00，对应字符串 "rld"，与 "Wo" 拼接为 "World"。

6. 如果 RISC-V 是大端存储，为得到相同输出，i 应设置为 0x726c6400。不需要更改 57616，因为 %x 与字节序无关。

7. 在 printf("x=%d y=%d", 3) 中，"y=" 之后将打印栈上的随机值。因为格式字符串期望两个参数，但只提供了一个，第二个参数未定义。
```

---
# Backtrace(moderate)

**_kernel/printf.c_**中

```
void
backtrace(void)
{
    printf("backtrace:\n");
    uint64 fp = r_fp();
    while (PGROUNDUP(fp) - PGROUNDDOWN(fp) == PGSIZE)
    {
        uint64 ret_addr = *(uint64*)(fp - 8);
        printf("%p\n", ret_addr);
        fp = *(uint64*)(fp - 16);
    }
    /*法二：
    uint64 kstack = myproc()->kstack;
    for (uint64 fp = r_fp(); PGROUNDDOWN(fp) == kstack; fp = *((uint64 *)(fp - 16)))
    {
        printf("%p\n", *((uint64 *)(fp - 8)));
    }
    */
}

void
panic(char *s)
{
  pr.locking = 0;
  printf("panic: ");
  printf(s);
  printf("\n");
  backtrace(); //添加
  panicked = 1; // freeze uart output from other CPUs
  for(;;)
    ;
}
```

sysproc.c中
```
uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  backtrace(); //添加
  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}
```
- GCC编译器将当前正在执行的函数的帧指针保存在`s0`寄存器，将下面的函数添加到**_kernel/riscv.h_**
```
static inline uint64
r_fp()
{
  uint64 x;
  asm volatile("mv %0, s0" : "=r" (x) );
  return x;
}

```

defs.h
```
// printf.c
...
void            backtrace(void);
...
```
---

# Alarm(Hard)

这项练习要实现定期的警报。首先是要通过`test0`，如何调用处理程序是主要的问题。程序计数器的过程是这样的：

1. `ecall`指令中将PC保存到SEPC
2. 在`usertrap`中将SEPC保存到`p->trapframe->epc`
3. `p->trapframe->epc`加4指向下一条指令
4. 执行系统调用
5. 在`usertrapret`中将SEPC改写为`p->trapframe->epc`中的值
6. 在`sret`中将PC设置为SEPC的值

可见执行系统调用后返回到用户空间继续执行的指令地址是由`p->trapframe->epc`决定的，因此在`usertrap`中主要就是完成它的设置工作。

提要：调用链
alarmtest -> sigalarm ->(通过 ecall 进入内核) -> (trampoline.S 中的) uservec -> usertrap -> sys_sigalarm -> usertrapret(返回用户态) 并执行 handler -> sigreturn -> (通过 ecall 进入内核) -> (trampoline.S 中的) uservec -> usertrap ->sys_sigreturn -> usertrapret(返回用户态) 

## test0: invoke handler(调用处理程序)

(1) 在 user目录下新建文件 sigalarm.c z和 sigreturn.c
(2) 在 user.h 中添加声明
```
int sigalarm(int ticks, void (*handler)());
int sigreturn(void);
```
(3) 在 usys.pl 中添加这俩个存根
(4) 在 kernel/syscall.h 添加新增的两个系统调用号
(5) 在 kernel/syscall.c 中实现函数 sys_sigalarm 和 sys_sigreturn
(6) 在 Makefile 中的 UPROGS 字段添加 alarmtest
(7) 在`struct proc`中增加字段，同时记得在`allocproc`中将它们初始化为0，并在`freeproc`中也设为0

`struct proc`：
```
int alarm_interval;          // 报警间隔
void (*alarm_handler)();     // 报警处理函数
int ticks_count;             // 两次报警间的滴答计数

```

`allocproc`和`freeproc`：
```
/**
 * allocproc.c
 */
p->alarm_interval = 0;
p->alarm_handler = 0;
p->ticks_count = 0;

/**
 * freeproc.c
 */
p->alarm_interval = 0;
p->alarm_handler = 0;
p->ticks_count = 0;
```

(8) `sys_sigalarm()`将报警间隔和指向处理程序函数的指针存储在`struct proc`的新字段中
```
uint64
sys_sigalarm(void)
{
    if(argint(0, &myproc()->alarm_interval) < 0 || argaddr(1, (uint64*)&myproc()->alarm_handler) < 0)
        return -1;
    return 0;
}
```

(9)目前来说，`sys_sigreturn`系统调用返回应该是零
```
uint64
sys_sigreturn(void)
{
	return 0;
}
```

(10) 每一个滴答声，硬件时钟就会强制一个中断，这个中断在kernel/trap.c 中的`usertrap()`中处理，修改usertrap()
```
//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void
usertrap(void)
{
  ...
  
  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2){
      if(p->alarm_interval != 0 && ++p->ticks_count == p->alarm_interval){
        p->trapframe->epc = (uint64)p->alarm_handler;
        p->ticks_count = 0;
      }
      yield();
  }
  usertrapret();
}
```

## test1/test2(): resume interrupted code(恢复被中断的代码)

接下来要通过`test1`和`test2`，要解决的主要问题是寄存器保存恢复和防止重复执行的问题。考虑一下没有alarm时运行的大致过程

1. 进入内核空间，保存用户寄存器到进程陷阱帧
2. 陷阱处理过程
3. 恢复用户寄存器，返回用户空间

而当添加了alarm后，变成了以下过程

1. 进入内核空间，保存用户寄存器到进程陷阱帧
2. 陷阱处理过程
3. 恢复用户寄存器，返回用户空间，但此时返回的并不是进入陷阱时的程序地址，而是处理函数`handler`的地址，而`handler`可能会改变用户寄存器

怎么解决`handler`可能会改变用户寄存器的问题？
我们要在`usertrap`中再次保存用户寄存器，当`handler`调用`sigreturn`时将其恢复，并且要防止在`handler`执行过程中重复调用，


(1) 为了当计时器关闭时，让`usertrap`在`struct proc`中保存足够的状态，以使`sigreturn`可以正确返回中断的用户代码，再在`struct proc`中新增两个字段

```
int is_alarming;                    // 是否正在执行告警处理函数
struct trapframe* alarm_trapframe;  // 告警陷阱帧
```

(2) 在allocproc和freeproc中添加相关分配，回收内存的代码
```
/**
 * allocproc.c
 */
// 初始化告警字段
if((p->alarm_trapframe = (struct trapframe*)kalloc()) == 0) {
    freeproc(p);
    release(&p->lock);
    return 0;
}
p->is_alarming = 0;
p->alarm_interval = 0;
p->alarm_handler = 0;
p->ticks_count = 0;

/**
 * freeproc.c
 */
if(p->alarm_trapframe)
    kfree((void*)p->alarm_trapframe);
p->alarm_trapframe = 0;
p->is_alarming = 0;
p->alarm_interval = 0;
p->alarm_handler = 0;
p->ticks_count = 0;

```

(3) 更改 `sys_sigalarm`和`sys_sigreturn`，恢复陷阱帧
```
uint64
sys_sigalarm(void)
{
    if(argint(0, &myproc()->alarm_interval) < 0 || argaddr(1, (uint64*)&myproc()->alarm_handler) < 0)
        return -1;
    myproc()->is_alarming = 0;
    return 0;
}

uint64
sys_sigreturn(void)
{
  memmove(myproc()->trapframe, myproc()->alarm_trapframe, sizeof(struct trapframe));
  myproc()->is_alarming = 0;
  intr_on(); //这句代码舍去可以通过测试但是最好添加
  return 0;
}
```

(4) 更改usertrap函数，保存进程陷阱帧`p->trapframe`到`p->alarm_trapframe`

```

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void
usertrap(void)
{
  ...

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2){
      if(p->alarm_interval != 0 && ++p->ticks_count == p->alarm_interval){
          if(!p->is_alarming){
            memmove(p->alarm_trapframe, p->trapframe, sizeof(struct trapframe));
            p->trapframe->epc = (uint64)p->alarm_handler;
            p->ticks_count = 0;
            p->is_alarming = 1;
            intr_off(); //这句代码舍去可以通过测试但是不严谨
        }
      }
      yield();
  }
  usertrapret();
}
```

---
这些思考还存在未解决的问题！！！！

==疑问：usertrap中的intr_off(); 是否可以省去？==

（我当时的理解：useutrap中的is_alarming可以防止重复调用，当在用户态处理 handler函数时，若又触发了中断，is_alarming字段还是为1,所以不会重复调用；）


 分析与验证
1. 去掉 intr_off() 的影响
- 作用回顾：
    - intr_off() 禁用中断（sstatus.SIE = 0），防止 handler 执行期间嵌套定时器中断。
- 移除后的行为：
    - 中断仍可能触发：
        - usertrapret 调用 w_sstatus(p->trapframe->sstatus)，若原始用户态 SIE == 1，返回用户态后中断启用。
        - handler（如 periodic）执行 printf 时，新定时器中断可能发生。
    - 但未重复调用：
        - usertrap 检查 if(!p->is_alarming)，若 is_alarming == 1，跳过设置 epc，仅执行 yield()。
    - 我的理解部分正确：
    - is_alarming 确实在内核态阻止了重复设置 epc，即使中断触发，usertrap 不会跳转到 handler。
- 潜在风险：
    - **若 handler 执行时间长于 alarm_interval（如大量 I/O），多次中断累积可能非常影响 ticks_count 的准确性**（测试只是侥幸通过，因为且测试运行的 count 只要求 > 0， 若放到 > 2 就会出错）。
    - 当前测试通过可能是 periodic 执行较快，未触发竞争。


==问: 为什么若 handler 执行时间长于 alarm_interval（如大量 I/O），多次中断累积可能影响 ticks_count 的准确性==

我举例说明：

 示例 1：无 intr_off() 

1. 初始状态：
    - ticks_count = 0，is_alarming = 0。  
2. 第 1-10 tick：
    - 每 tick ticks_count++。
    - 第 10 tick：  **
        - ++p->ticks_count = 10，触发条件。
        - !p->is_alarming 为真，设置 epc = periodic，ticks_count = 0，is_alarming = 1。  
        - 返回用户态，开始 periodic（执行至第 24 tick）。
3. 第 11-24 tick（periodic 执行中）**：
    - 每 tick 中断触发：
        - 第 11 tick：ticks_count = 1。  
        - 第 12 tick：ticks_count = 2。  
        - ...
        - 第 24 tick：ticks_count = 14。
    - is_alarming = 1，不重复触发，仅 yield()。
4. 第 25 tick：
    - periodic 调用 sigreturn，ticks_count = 15（递增后）。
    - is_alarming = 0，恢复原始 epc。
5. 第 26-34 tick：
    - ticks_count 继续递增：
        - 第 26 tick：ticks_count = 16。
        - ...
        - 第 34 tick：ticks_count = 24。
    - 未触发（需 ticks_count = 10）。
6. 第 35 tick：
    - ticks_count = 25，未触发。
    - **问题**：ticks_count 已超出预期周期。



示例 2：有 intr_off() 

1. 初始状态：
    - ticks_count = 0，is_alarming = 0。  
2. 第 1-10 tick：
    - ++p->ticks_count = 10，触发。
    - ticks_count = 0，is_alarming = 1，intr_off()。
    - 开始 periodic（至第 24 tick）。
3. 第 11-24 tick：
    - 中断禁用，ticks_count = 0 不变。
4. 第 25 tick：
    - sigreturn 执行，is_alarming = 0，intr_on()。
    - ticks_count = 1（第 25 tick 递增）。
5. 第 26-34 tick：
    - ticks_count 递增：
        - 第 26 tick：ticks_count = 2。
        - ...
        - 第 34 tick：ticks_count = 10。
6. 第 35 tick：
    - ++p->ticks_count = 10，触发 periodic。

结果
- 周期：
    - 25 tick（15 tick 执行 + 10 tick 间隔）。
- 准确性：
    - 周期包含执行时间，但从 sigreturn 后重新计数，更可预测。


???
==思考：导致上述问题的本质是 ticks_count 计数设置问题，除了添加 intr_on(); 是否还有别的解决方法？==

我想到可以在 sys_sigreturn函数中添加 myproc()->ticks_count = 0，也可以达到预期效果。

???
==继续思考：哪种方法效率更高（设置 intr_off 和 清理计时器）==

- 方法 1（推荐）：
    - 禁用中断，减少内核态处理开销。
- 方法 2：
    - 中断持续触发，usertrap 多次执行 ++p->ticks_count 和 yield()，稍增加开销。


***==没有解决的问题：通过原测试的代码的原因测试只要求了 score > 0， 若改为了 score >1 就不能通过 test2，而且我已经添加了 intr_off 和 清理计时器的逻辑，我反思 intr_off 不起作用的原因或许是 usertrapret 时又恢复了状态，sys_sigreturn 清理计时器后仍然有可能发生中断？==***

---

==疑问：sys_sigreturn中的intr_on(); 是否可以省去？==

（我的理解：哪怕没有在 sys_sigreturn 函数中设置开中断，sys_sigreturn 恢复 trapframe，usertrapret 自动恢复原始 SIE 为1 ，即启动开中断）

回答：
- usertrapret 的 w_sstatus 确保了中断恢复，理解正确

---


总结精华，完整的陷阱处理过程以及重要细节
（以系统调用 write 为例）

在 shell 输入 write(1, "hello",  5);
↓
在 usys.S 中找到并执行 write 函数
↓
write 函数：保存系统调用号到 a7 寄存器，执行 ecall 指令
↓
执行 ecall 指令
↓
ecall 指令：将当前的 pc 保存到 setp 寄存器中，更改 mode 寄存器内容切换模式（从用户到管理员），将 stvec 设置为 trampoline page 代码开始的位置（trampoline page 处包含 uservec 代码）
↓
==由于 ecall 指令并不切换页表，所以现在用户空间，但却是管理员模式，这就说明我们需要在用户空间的某个位置可以执行内核代码，trampoline page 应运而生。==

==内核非常方便的将trapframe page映射到了每个user page table，用于用户空间和内核空间跳转页面==
↓
跳转到 stvec 所指的位置即 trampoline page 的位置执行
↓
首先获得当前进程陷阱帧的地址，然后将用户寄存器中的参数保存在陷阱帧中，切换页表，获得 usertrap 的地址，跳转到 usertrap 函数执行
↓
==之所以用户寄存器（User Registers）必须在汇编代码中保存，是因为任何需要经过编译器的语言，例如C语言，都不能修改任何用户寄存器。所以对于用户寄存器，必须要在进入C代码之前在汇编代码中保存好。==

==之所以切换页表地址转换仍然不会出现错误==
==，是因为trampoline代码在用户空间和内核空间都映射到了同一个地址。==
↓
usertrap 函数：设置 stvec 保存 kernelvec 的起始地址，将 setp 的值保存到陷阱帧中，判断陷阱类型从而进行处理

系统调用：执行相应的系统调用函数
异常：杀死进程
中断（除时钟中断以外）：中断处理
时钟中断：特殊处理
↓
（以 write 系统调用为例）
↓
usertrap 调用 syscall 函数，syscall 函数取出陷阱帧中保存的 a7 的值即系统调用号，调用相应的函数（如 sys_write），系统调用函数返回到 usertrap 函数。
↓
usertrap 接着调用 usertrapret 函数
↓
usertrapret 函数将内核环境的值（内核页表的指针，内核栈的地址，usertrap 的指针）保存到陷阱帧，恢复 sstatus 寄存器状态，得到 trapframe 和用户页表的地址一起作为参数传递给 userret
↓
==之所以只是取得用户页表的地址而没有直接用 w_stap 切换页表，是因为此时我们处于内核页表的位置，且不是在 trampoline 这样的位置只是在一段普通的内核的 c 代码中，如果这是切换回了用户页表，地址映射可能会出错==

==传入userret的参数 trapframe 和用户页表的地址存到了寄存器 a0 和 a1 中==
↓
userret 函数：切换页表，将陷阱帧（此时保存在 a0 中），中保存的参数恢复到用户寄存器中，sret 返回
↓
返回到 usys.S 中，继续执行 ret 返回到 shell
