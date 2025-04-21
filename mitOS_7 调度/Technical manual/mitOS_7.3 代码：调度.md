###  重点总结

1. **调度流程**：
    - 进程通过 sched 放弃 CPU，切换到 scheduler，再到新进程。
2. **约定**：
    - 持 p->lock，更新状态，调用 sched。
3. **锁特殊性**：
    - swtch 持锁，***跨线程释放，防栈冲突***。
4. **协程**：
    - sched 和 scheduler 循环切换。
5. **新进程**：
    - 从 forkret 开始，释放锁。
6. **调度循环**：
    - scheduler 找 RUNNABLE，设 RUNNING，运行。  
7. **不变量**：
    - RUNNING 和 RUNNABLE 状态受 p->lock 保护。
8. **锁多功能**：
    - 保护切换、exit、wait 等，可能需拆分。

---

### 在 xv6 中的体现

- **代码**：
    - kernel/proc.c:515：yield。  
    - kernel/proc.c:499：sched 检查。  
    - kernel/proc.c:457：scheduler 循环。  
- **结构**：
    - p->lock 是核心***同步***工具。
    
---

#### 1. **调度器的角色与切换流程**

- **描述**：
    - ***每个 CPU 有调度器线程（scheduler 函数）***，选择并运行下一个进程。进程通过 sched 放弃 CPU，切换到调度器。
- **类比**：
    - 想象一个只有一个工作台（CPU）的木匠作坊，经理（调度器）负责安排木匠（进程）。木匠 A 累了（yield），喊经理接管（sched），经理记下 A 的状态（swtch），自己上场挑选下一个木匠 B。
- **解释**：
    - 调度器是独立线程，协调进程轮换。

#### 2. **放弃 CPU 的约定**

- **描述**：
    - 进程需持 p->lock，释放其他锁，更新状态（如 RUNNABLE），调用 sched。yield、sleep、exit 遵循此规则。
- **类比**：
    - 木匠 A 要休息，锁住自己工具箱（p->lock），放下其他工具（释放锁），告诉经理“我休息”（状态更新），请经理安排（sched）。
- **解释**：
    - 约定***确保切换前状态一致，锁保护关键数据***。

#### 3. **锁的特殊用法**

- **描述**：
    - ***swtch 调用时持 p->lock，锁控制权传给新线程***，打破常规（通常由同一线程释放锁）。
- **类比**：
    - 木匠 A 锁住工具箱交给经理（swtch），经理带着锁接管，而不是 A 自己解锁。常规是 A 锁后自己开，但这里不行。
- **解释**：
    - 锁跨线程传递，确保切换期间状态安全。

#### 4. **为何持锁调用 swtch**

- **描述**：
    - 若不持 p->lock，yield 设为 RUNNABLE 后，另一 CPU 可能运行进程，导致***栈冲突***。
- **类比**：
    - A 说“我休息”后未锁工具箱，另一经理（CPU）让 A 在旧工作台上开工，两个 A 同时用一台（栈冲突）。
- **解释**：
    - p->lock 防止并发运行，确保栈独占。

#### 5. **协程模式**

- **描述**：
    - 内核线程在 sched 放弃 CPU，切换到 scheduler，再回到 sched，形如协程（479 -> 517 -> 479）。
- **类比**：
    - A 喊经理（sched），经理接手（scheduler），再喊回 A，循环像两人交替递话筒（协程）。
- **解释**：
    - ***固定切换点*** 形成简单模式。

#### 6. **新进程的特殊情况**

- **描述**：
    - 新进程从 forkret 开始，释放 p->lock，而非 sched。
- **类比**：
    - 新木匠 B 上场，经理直接给他工具（forkret），解锁后开工，不像老木匠走休息流程。
- **解释**：
    - ***新进程无需完整切换路径***。

#### 7. **调度器循环**

- **描述**：
    - scheduler 循环找 RUNNABLE 进程，设为 RUNNING，调用 swtch 运行。
- **类比**：
    - 经理检查名单（进程表），找到准备干活的 B，标记“开工”（RUNNING），给他工作台（swtch）。
- **解释**：
    - 调度器驱动进程执行。

#### 8. **不变量与锁**

- **描述**：
    - **RUNNING 不变量**：中断可安全切换，寄存器在 CPU，c->proc 指向进程。
    - **RUNNABLE 不变量**：调度器可安全运行，寄存器在 context，无 CPU 使用栈。
    - p->lock 在不变量不成立时持有。
- **类比**：
    - **RUNNING**：木匠 A 在干活，工具在台上，经理知道是 A。
    - **RUNNABLE**：A 休息，工具收好，无人用他的台。
    - 切换时锁住工具箱，直到状态稳定。
- **解释**：
    - 锁保护状态转换的中间态。

#### 9. **锁的跨线程释放**

- **描述**：
    - yield 获取 p->lock，scheduler 释放，确保不变量恢复。
- **类比**：
    - A 锁住工具箱说休息，经理接手后解锁，直到 B 安全开工。
- **解释**：
    - 跨线程锁传递维护一致性。

#### 10. **锁的其他功能**

- **描述**：
    - p->lock 还保护 exit、wait、避免 wakeup 丢失及状态争用。
- **类比**：
    - 工具箱锁不仅防切换混乱，还防 A 走时 B 偷看（exit vs. wait），或喊醒丢失。
- **解释**：
    - 锁多功能，可能需拆分优化。

---

### 类比举例：P1 到 P2

- **场景**：
    - P1 运行，定时器中断，切换到 P2。
- **过程**：
    1. **P1 yield**： 
        - P1 锁工具箱（p->lock），设休息（RUNNABLE），喊经理（sched）。
    2. **swtch**：
        - 保存 P1 工具单（p->context），切换到经理单（cpu->scheduler）。
    3. **scheduler**：  
        - 经理找 P2，标记开工（RUNNING），切换到 P2 单（swtch）。
    4. **P2 运行**：
        - P2 用自己工具开工，经理解锁。
- **结果**：
    - P1 到 P2，锁保护切换。
---
## 协程（479 -> 517 -> 479）是怎么实现？

```
            →   swtch()   →   进程代码执行
scheduler()（获得锁1）             ↓
            ←   swtch()   ←   sched()  → ←  yield()（获得锁1←，释放锁1→）
```
***从 yield() 获得锁 1 到释放锁 1 是一个完整的调度过程***

```
void sched(void) {
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
  swtch(&p->context, &mycpu()->context);  // 479
  mycpu()->intena = intena;               // 480
}
```


```
void scheduler(void) {
  struct proc *p;
  struct cpu *c = mycpu();

  c->proc = 0;
  for(;;) {
    intr_on();
    int nproc = 0;
    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state != UNUSED) {
        nproc++;
      }
      if(p->state == RUNNABLE) {
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);  // 517
        c->proc = 0;
      }
      release(&p->lock);
    }
    if(nproc <= 2) {
      intr_on();
      asm volatile("wfi");
    }
  }
}
```
#### 切换过程


1. **sched()放弃CPU**：  
    - 调用 swtch(&p->context, &mycpu()->context)（行479）。 
    - p->context.ra 保存 sched() 中 swtch() 后的地址（行480）（中断返回点）
    - 跳转到scheduler()
2. **scheduler()接管**：  
    - 从mycpu()->context恢复，从for循环前的 c->proc = 0 开始，运行调度器代码。
    - 找到 RUNNABLE进程，调用 swtch(&mycpu()->context, &p->context)（行517）
    - c->context.ra 保存 scheduler() 中 swtch() 后的地址（行518）（中断返回点）
    - 跳转回进程。
3. **进程执行完回到sched()**：
    - 从 p->context 保存的中断返回点恢复，ra 指向行480。
    - 执行 mycpu()->intena = intena，返回调用者（如yield()）

#### 协程特性

- **协作式**：sched() 主动调用 swtch() 让出控制权。
- **状态保存**：p->context 保存进程状态，恢复时从***断点*** 继续。
- **无抢占**：切换由程序控制，类似协程的 yield 和 resume。

#### 注意：
- **p->context.ra** 不是进程“执行完”的返回地址，而是上次切换出时的断点。
- scheduler() 的 swtch() 是单向跳转，恢复靠另一端的 swtch()

