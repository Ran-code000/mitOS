:
###  重点总结

1. **多路复用步骤**：
    - 用户到内核、内核到调度器、调度器到新进程、新进程返回用户态。
2. **调度器线程**：
    - 每个 CPU 有独立栈，避免争用。
3. **上下文切换**：
    - 保存旧寄存器，恢复新寄存器，swtch 执行。
4. **保存与恢复**：
    - swtch 处理***被调用方***寄存器，ra 控制返回。
5. **流程**：
    - usertrap -> yield -> sched -> swtch -> scheduler。
6. **复杂性**：
    - 低级实现（如 swtch.S）不透明但关键。

---

### 在 xv6 中的体现

- **代码**：
    - kernel/swtch.S：swtch 实现。  
    - kernel/proc.c：yield、sched、scheduler。
    - kernel/proc.h：struct context 定义。  
- **结构**：
    - struct proc 含 context，struct cpu 含 scheduler。  
---

#### 1. **上下文切换的步骤概述**
![[Pasted image 20250402105640.png]]
- **描述**：
    - 图：用户进程切换涉及**用户到内核（中断/系统调用）、内核到调度器、调度器到*新进程内核线程*、新进程返回用户态**。
- **类比**：
    - 想象两个工人（进程）在工厂（CPU）轮流使用工具台（寄存器和栈）。步骤包括：工人 A 被叫到办公室（内核），经理（调度器）接管，安排工人 B 上场，最后 B 回到工作区（用户态）。
- **解释**：
    - 上下文切换是多阶段过程，涉及用户态和内核态的转换。

#### 2. **调度器线程的必要性**

- **描述**：
    - 调度器不在旧进程栈上运行，***每个 CPU 有专用调度器线程（含独立栈和寄存器），避免多核竞争同一栈***。
- **类比**：
    - 经理不用工人 A 的桌子（栈），而是有自己的办公桌（调度器栈）。若多个经理（CPU）共用 A 的桌子，同时操作会混乱。
- **解释**：
    - 独立调度器栈（如 cpu->scheduler）确保安全性和并发性。

#### 3. **上下文切换的核心：保存与恢复**

- **描述**：
    - ***切换线程需保存旧线程寄存器，恢复新线程寄存器***，包括栈指针（sp）和程序计数器（间接通过 ra）。
- **类比**：
    - 工人 A 暂停，经理记下他的工具位置和当前任务（保存寄存器），拿出工人 B 的记录（恢复寄存器），B 在自己工具台上继续。
- **解释**：
    - 寄存器切换***改变执行流和栈位置***。

#### 4. **swtch 函数的作用**

- **描述**：
    - swtch 保存旧上下文到 struct context *old，恢复新上下文从 struct context *new，不直接保存 PC，但保存 ra（返回地址）*。
- **类比**：
    - 经理把工人 A 的工具单（old）填好，从工人 B 的工具单（new）取出记录。A 的单记下“去哪儿继续”（ra），B 从上次暂停处开工。
- **解释**：
    - swtch 是低级汇编，处理**寄存器**保存和恢复。

#### 5. **从进程到调度器的切换**

- **描述**：
    - 中断后 usertrap 调用 yield，yield 调用 sched，sched 调用 swtch，保存进程上下文，切换到 cpu->scheduler。
- **类比**：
    - 工人 A 被铃声打断（中断），经理喊他停（yield），记下状态（sched），拿出自己的办公单（swtch 到调度器）。
- **解释**：
    - 进程主动放弃 CPU，进入调度器。

#### 6. **被调用方保存寄存器**

- **描述**：
    - swtch 只保存被调用方保存寄存器（callee-saved），调用方保存寄存器由 C 代码处理。
- **类比**：
    - 经理只记工人长期工具（callee-saved，如 s0-s11），临时工具（caller-saved，如 a0-a7）由工人自己放桌上（栈）。
- **解释**：
    - 遵循调用约定，减少 swtch 负担。

#### 7. **返回到新线程**

- **描述**：
    - swtch 恢复新上下文后，返回到 ra 指定的地址，在新栈上执行。
- **类比**：
    - 经理按 B 的工具单恢复工具，B 从上次停处（ra）继续，在自己桌上（新栈）工作。
- **解释**：
    - 返回地址驱动线程切换后的执行流。

#### 8. **示例：cc 切换到 ls**

- **描述**：
    - cc 的内核线程保存上下文，切换到 ls 的内核线程（RUNNABLE），恢复 ls 的 context 和 trapframe。
- **类比**：
    - 工人 A（cc）停工，经理记下状态，拿出工人 B（ls）的记录（context），恢复 B 的工具和半成品（trapframe），B 继续干活。
- **解释**：
    - 已运行的进程状态被完整恢复。

#### 9. **调度器返回**

- **描述**：
    - ***swtch 切换到 cpu->scheduler，返回到 scheduler 函数，而非 sched***。
- **类比**：
    - 经理接管后，回到自己的办公流程（scheduler），而非工人 A 的暂停处（sched）。
- **解释**：
    - 调度器栈独立运行后续逻辑。

---

### 类比举例：从 P1 到 P2

- **场景**：
    - P1（用户进程）运行，定时器中断，切换到 P2。
- **过程**：
    1. **中断**：
        - P1 执行，铃响（定时器），进入 usertrap。
    2. **yield**：
        - usertrap 调用 yield，P1 决定暂停。
    3. **sched**：
        - yield 调用 sched，准备切换。
    4. **swtch**：
        - sched 调用 swtch，保存 P1 的 context（s0、sp、ra 等），恢复 cpu->scheduler。
    5. **调度器**：
        - ***返回到 scheduler***，选择 P2，swtch 恢复 P2 的 context。
    6. **P2 执行**：
        - P2 从上次暂停处继续，恢复 trapframe，回到用户态。
- **结果**：
    - P1 到调度器，再到 P2，无缝切换。

---

## 代码分析

- **scheduler()**：永不返回的调度循环，选择并运行进程。
- **yield()**：进程主动让出CPU。
- **sched()**：安全地将控制权交回调度器。
- **swtch()**：底层的上下文切换实现。
```
             →  swtch() →  进程代码
  scheduler()                  ↓
             ←  swtch() ←  sched()  ←  yield()
```
#### 时间线

1. **调度器**：scheduler() → swtch(&c->context, &p->context)。  
2. **进程**：运行 → yield() → sched() → swtch(&p->context, &c->context)。  
3. **调度器**：scheduler()继续。

### 1. scheduler(void)  

**功能**: ***每个CPU的进程调度器，永不返回***，循环执行以下步骤：

- 选择一个可运行的进程。
- 切换到该进程运行。
- 当进程让出控制权时，回到调度器。
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

    int nproc = 0;
    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state != UNUSED) {
        nproc++;
      }
      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
      release(&p->lock);
    }
    if(nproc <= 2) {   // only init and sh exist
      intr_on();
      asm volatile("wfi");
    }
  }
}
```
**代码解释**:

- **struct cpu *c = mycpu();**:  
    - 获取当前CPU的结构体，c->proc表示当前CPU正在运行的进程。
- **c->proc = 0;**:  
    - 初始化时，当前CPU没有运行任何进程。
- **for(;;)**:  
    - ***无限循环，调度器永不退出***。
- **intr_on();**:  
    - ***打开中断，避免死锁***。设备中断（如定时器）可以触发抢占，***确保调度公平性，避免调度器独占CPU***
- **for(p = proc; p < &proc[NPROC]; p++)**:  
    - 遍历全局进程表，检查每个进程的状态。
- **acquire(&p->lock);**:  
    - 获取进程锁，确保状态检查和修改是线程安全的。
- **if(p->state != UNUSED)**:  
    - 统计活跃进程数（nproc），用于后续判断。
- **if(p->state == RUNNABLE)**:  
    - 如果进程是“可运行”状态：
        - **p->state = RUNNING;**：标记为“运行中”。
        - **c->proc = p;**：记录当前CPU运行的进程。
        - **swtch(&c->context, &p->context);**：  
            - 上下文切换，从调度器的上下文切换到进程的上下文，开始运行该进程。
        - **返回后**：
            - 进程通过swtch()回到调度器，c->proc = 0;清除当前进程记录。
- **release(&p->lock);**：  
    - 释放进程锁。进程运行时会释放自己的锁，回到调度器时需重新获取。
- **if(nproc <= 2)**:  
    - 如果活跃进程数≤2（通常只有init和sh），系统进入空闲状态：
        - **intr_on();**：确保中断开启。
        - **asm volatile("wfi");**：执行“等待中断”（Wait For Interrupt），让CPU休眠以节省功耗，直到中断（如定时器）唤醒。

**关键点**:
- ***调度器是每个CPU的独立线程***，循环寻找RUNNABLE进程并运行。
- ***进程负责在返回调度器前修改自己的状态***（例如通过yield()变为RUNNABLE）。


---
### 2. yield(void)  

**功能**: 当前进程主动放弃CPU一个调度周期。
```
// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}
```
**代码解释**:

- **struct proc *p = myproc();**：  
    - 获取当前进程。
- **acquire(&p->lock);**：  
    - 获取进程锁，保护状态修改。
- **p->state = RUNNABLE;**：
    - 将状态改为“可运行”，表示愿意让出CPU。
- **sched();**：  
    - 调用sched()切换到调度器。
- **release(&p->lock);**：  
    - 返回后释放锁。

**用途**:
- 用于***协作式调度***，或***响应定时器中断***（如前文if(which_dev == 2) yield();）。
---

### 3. sched(void) 

**功能**: 将当前进程切换回调度器。调用时必须持有p->lock，且已修改进程状态。

***sched()中的检查防止错误状态下的切换***
```
// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}
```
**代码解释**:

- **struct proc *p = myproc();**：  
    - 获取当前进程。
- **检查条件**：
    - **if(!holding(&p->lock))**：确保持有进程锁，否则崩溃。
    - **if(mycpu()->noff != 1)**：检查锁嵌套计数，确保只持有一个锁（避免死锁）。
    - **if(p->state == RUNNING)**：当前进程不能是“运行中”状态（应为RUNNABLE或SLEEPING）。
    - **if(intr_get())**：不能在中断开启时调用（避免中断干扰上下文切换）。
- **intena = mycpu()->intena;**：  
    - 保存当前CPU的中断状态（intena表示中断是否启用）。
- **swtch(&p->context, &mycpu()->context);**：  
    - 上下文切换，从进程的上下文切换到调度器的上下文。
- **mycpu()->intena = intena;**：  
    - 返回后恢复中断状态。

**关键点**:
- sched()是进程***主动让出CPU***的桥梁，确保切换安全。
- **中断状态保存在CPU结构体中（而不是进程中），因为它是*内核* 线程的属性**。
- ***intr_on()和intena确保调度器和进程切换时中断状态正确***

---

### 4. swtch(struct context *old, struct context *new)  

**功能**: 上下文切换的汇编实现，将当前上下文保存到old，从new加载新上下文。
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
**代码解释**:

- **输入**：
    - a0：指向旧上下文的指针（struct context *old）。
    - a1：指向新上下文的指针（struct context *new）。
- **保存寄存器**：
    - sd ra, 0(a0)：***保存返回地址（ra）到old的偏移0***。
    - sd sp, 8(a0)：保存栈指针（sp）到偏移8。
    - sd s0, 16(a0) 至 sd s11, 104(a0)：保存被调用者保存寄存器（s0-s11）。
- **加载寄存器**：
    - ld ra, 0(a1)：***从new加载返回地址***。
    - ld sp, 8(a1)：加载新栈指针。
    - ld s0, 16(a1) 至 ld s11, 104(a1)：加载新寄存器值。
- **ret**：
    - 返回到新上下文的ra地址，开始执行新代码。

**关键点**:
- struct context包含ra（返回地址）、sp（栈指针）和s0-s11（保存寄存器）。
- 切换后，CPU从新上下文继续执行，旧上下文被保存以便将来恢复。

---

### 综合分析

#### 调度流程

1. **调度器启动**：
    - scheduler()运行在每个CPU上，寻找RUNNABLE进程。
2. **选择进程**：
    - 找到后，将其状态改为RUNNING，通过swtch()切换到进程上下文。
3. **进程运行**：
    - 进程执行用户代码或内核代码，直到调用yield()或被中断。
4. **返回调度器**：
    - yield()调用sched()，通过swtch()切换回调度器上下文。
5. **循环**：
    - 调度器继续寻找下一个进程。

#### 线程安全
- **p->lock**：
    - 保护进程状态（state）和上下文（context），确保修改和切换是***原子***的。
- **intr_on()**：  
    - ***允许中断，避免调度器独占CPU***。
- **检查条件**：
    - ***sched()中的检查防止错误状态下的切换***。

#### 优化
- **wfi**：
    - 当系统空闲时（nproc <= 2），CPU休眠，等待中断唤醒。
- **中断状态管理**：
    - intena保存和恢复，***确保切换不影响中断行为***。


