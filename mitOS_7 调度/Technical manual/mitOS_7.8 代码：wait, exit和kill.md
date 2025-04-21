### 重点总结

1. **wait**：
    - 持 p->lock 扫描，清理 ZOMBIE 子进程或睡眠。
2. **exit**：
    - 设 ZOMBIE，唤醒父进程，转移子进程给 init。
3. **锁顺序**：
    - 父→子，避免死锁。
4. **np->parent**：
    - 无锁访问，因父控制安全。
5. **wakeup1**：  
    - 提前唤醒父进程，循环保护。
6. **kill**：  
    - 设 p->killed，唤醒睡眠进程，目标自行退出。
7. **sleep 与 kill**：
    - 循环检查 p->killed，部分场景延迟退出。

---

### 在 xv6 中的体现

- **代码**：
    - kernel/proc.c:398：wait。  
    - kernel/proc.c:333：exit。  
    - kernel/proc.c:611：kill。  
- **实现**：
    - p->lock 协调，p->state 管理。
---

#### 1. **wait 和 exit 的同步**

- **描述**：
    - 子进程 exit 进入 ZOMBIE 状态，父进程 wait 观察并清理，可能晚于 exit。
- **类比**：
    - 孩子（子进程）完成作业（exit），变成“僵尸”等着家长（父进程）检查（wait）。家长可能在忙，等会才看。
- **解释**：
    - wait 和 exit 协调子进程终止。

#### 2. **wait 的实现**

- **描述**：
    - wait 持 p->lock（条件锁），扫描进程表，找到 ZOMBIE 子进程则清理，否则睡在 p->lock 上。
- **类比**：
    - 家长锁门（p->lock），检查孩子作业（进程表），找到完成的（ZOMBIE）就收拾，若没完成睡沙发（sleep）。
- **解释**：
    - p->lock 防丢失唤醒，循环处理等待。

#### 3. **锁顺序与死锁**

- **描述**：
    - wait 先持父锁再子锁，须全局一致避免死锁。
- **类比**：
    - 家长先锁自己房间（父锁），再检查孩子房间（子锁）。若顺序乱，可能卡在门口（死锁）。
- **解释**：
    - 统一锁顺序（如父→子）防死锁。

#### 4. **np->parent 的无锁访问**

- **描述**：
    - wait 检查 np->parent 不持 np->lock，因父进程控制 parent，安全。
- **类比**：
    - 家长看孩子写的“家长是谁”（np->parent），不用锁孩子门，因只有家长能改。
- **解释**：
    - 无锁访问简化，但依赖父进程控制。

#### 5. **exit 的流程**

- **描述**：
    - exit 记录状态码，释放资源，转移子进程给 init，唤醒父进程，设为 ZOMBIE，放弃 CPU。
- **类比**：
    - 孩子收拾书包（记录状态），把弟弟交给爷爷（init），喊醒家长（wakeup），变“僵尸”等收拾。
- **解释**：
    - exit 准备终止，通知父进程。

#### 6. **exit 的锁与唤醒**

- **描述**：
    - exit 持父锁和子锁唤醒，顺序同 wait，避免丢失唤醒和提前释放。
- **类比**：
    - 孩子锁家长门（父锁）和自己门（子锁），喊醒家长，确保家长看到“僵尸”前不收拾。
- **解释**：
    - 锁保护状态转换和通知。

#### 7. **wakeup1 的特殊性**

- **描述**：
    - wakeup1 只唤醒等待中的父进程，提前唤醒安全，因 wait 循环检查。
- **类比**：
    - 孩子喊家长前变“僵尸”，家长醒来检查（循环），不会误收拾。
- **解释**：
    - 提前唤醒不影响正确性。

#### 8. **kill 的实现**

- **描述**：
    - kill 设 p->killed，若目标睡眠则唤醒，目标自行 exit。
- **类比**：
    - 老师（kill）给学生贴“下课”条（p->killed），若学生睡着就喊醒，学生自己走（exit）。
- **解释**：
    - kill 简单标记，目标处理终止。

#### 9. **sleep 中的 kill 处理**

- **描述**：
    - kill 唤醒睡眠进程，循环检查 p->killed，放弃操作。
- **类比**：
    - 学生睡等面包，老师喊醒贴条，学生检查没面包且有条，走人。
- **解释**：
    - 循环确保条件和终止状态一致。

#### 10. **不检查 p->killed 的情况**

- **描述**：
    - 某些睡眠（如 virtio 磁盘）不检查 p->killed，需完成原子操作。
- **类比**：
    - 学生睡等烤面包，老师贴条，但学生坚持烤完再走（原子操作）。
- **解释**：
    - 保证系统调用完整性。

---

### 类比举例：父子进程交互

- **场景**：
    - 子进程 C1 调用 exit，父进程 P1 调用 wait，另一进程 P2 调用 kill。
- **过程**：
    1. **C1 exit**：
        - C1 锁 P1 门（父锁）和自己门（子锁），喊醒 P1（wakeup1），变“僵尸”。
    2. **P1 wait**：
        - P1 锁自己门，睡沙发（sleep），醒来见 C1“僵尸”，收拾，解锁。
    3. **P2 kill C1**：
        - C1 睡时，P2 贴条（p->killed），喊醒，C1 检查条走人（exit）。
- **结果**：
    - C1 安全终止，P1 清理，kill 生效。


---
### exit 函数
```
// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  // we might re-parent a child to init. we can't be precise about
  // waking up init, since we can't acquire its lock once we've
  // acquired any other proc lock. so wake up init whether that's
  // necessary or not. init may miss this wakeup, but that seems
  // harmless.
  acquire(&initproc->lock);
  wakeup1(initproc);
  release(&initproc->lock);

  // grab a copy of p->parent, to ensure that we unlock the same
  // parent we locked. in case our parent gives us away to init while
  // we're waiting for the parent lock. we may then race with an
  // exiting parent, but the result will be a harmless spurious wakeup
  // to a dead or wrong process; proc structs are never re-allocated
  // as anything else.
  acquire(&p->lock);
  struct proc *original_parent = p->parent;
  release(&p->lock);

  // we need the parent's lock in order to wake it up from wait().
  // the parent-then-child rule says we have to lock it first.
  acquire(&original_parent->lock);

  acquire(&p->lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup1(original_parent);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&original_parent->lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}
```
#### 工作流程

1. **获取当前进程并检查特殊情况**：
    - struct proc *p = myproc()：获取当前进程的结构体。
    - 如果是 initproc（初始进程，通常是 PID 为 1 的进程），调用 panic("init exiting")，因为初始进程不允许退出。
2. **关闭所有打开的文件**：
    - 遍历进程的文件描述符表（p->ofile），关闭每个打开的文件（fileclose），并将对应槽置为 0。
    - NOFILE 是进程能打开的最大文件数。
3. **释放当前工作目录**：
    - begin_op() 和 end_op()：开始和结束文件系统操作，确保同步。
    - iput(p->cwd)：释放当前工作目录的 inode。
    - p->cwd = 0：清空当前工作目录指针。
4. **唤醒 init 进程（可能的子进程重新归属）**：
    - 子进程可能会被重新分配给 initproc，所以提前唤醒它。
    - 由于锁的限制，不能精确判断是否需要唤醒，但无害的冗余唤醒是安全的：
        
5. **处理父进程锁和子进程重新归属**：
    - 获取当前进程的父进程指针副本（original_parent），避免父进程在等待期间被更改
    - 锁定父进程
    - 将当前进程的子进程重新分配给 init（reparent(p)）。
    - 唤醒可能在 wait 中睡眠的父进程（wakeup1(original_parent)）。
6. **设置进程状态**：
    - p->xstate = status：保存退出状态，供父进程通过 wait 获取。
    - p->state = ZOMBIE：将进程标记为僵尸状态。
7. **调度并退出**：
    - release(&original_parent->lock)：释放父进程的锁。
    - sched()：跳转到调度器，当前进程不再返回。
    - 如果 sched() 返回（不应该发生），调用 panic("zombie exit")。

#### 关键点

- 进程退出后变为僵尸状态，等待父进程回收。
- 文件和资源被清理，子进程被重新分配给 init。

---

### wait 函数

```

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  struct proc *np;
  int havekids, pid;
  struct proc *p = myproc();

  // hold p->lock for the whole time to avoid lost
  // wakeups from a child's exit().
  acquire(&p->lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(np = proc; np < &proc[NPROC]; np++){
      // this code uses np->parent without holding np->lock.
      // acquiring the lock first would cause a deadlock,
      // since np might be an ancestor, and we already hold p->lock.
      if(np->parent == p){
        // np->parent can't change between the check and the acquire()
        // because only the parent changes it, and we're the parent.
        acquire(&np->lock);
        havekids = 1;
        if(np->state == ZOMBIE){
          // Found one.
          pid = np->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&np->xstate,
                                  sizeof(np->xstate)) < 0) {
            release(&np->lock);
            release(&p->lock);
            return -1;
          }
          freeproc(np);
          release(&np->lock);
          release(&p->lock);
          return pid;
        }
        release(&np->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || p->killed){
      release(&p->lock);
      return -1;
    }

    // Wait for a child to exit.
    sleep(p, &p->lock);  //DOC: wait-sleep
  }
}
```

这个函数让当前进程等待任意一个子进程退出，并返回其 PID。参数 addr 是用户空间地址，用于存储子进程的退出状态。

#### 工作流程

1. **获取当前进程并加锁**：
    - struct proc *p = myproc()：获取当前进程。
    - acquire(&p->lock)：锁定当前进程，避免子进程退出时的唤醒丢失。
2. **无限循环查找子进程**：
    - for(;;)：持续扫描直到找到一个退出的子进程或无子进程。
    - 遍历进程表（proc 到 proc[NPROC]）：
        - 检查 np->parent == p，确认是否是当前进程的子进程。
        - 如果是子进程：
            - acquire(&np->lock)：锁定子进程。
            - 设置 havekids = 1，标记存在子进程。
            - 如果子进程是僵尸状态（np->state == ZOMBIE）：
                - 记录子进程 PID（pid = np->pid）。
                - 如果 addr != 0，将子进程退出状态（np->xstate）复制到用户空间（copyout）。
                - 如果复制失败，返回 -1。
                - 释放子进程资源（freeproc(np)）。
                - 释放锁并返回子进程 PID。
3. **无子进程或被杀死的情况**：
    - 如果 !havekids || p->killed，说明没有子进程或当前进程被杀死，返回 -1。
4. **等待子进程退出**：
    - 如果没有找到僵尸子进程，调用 sleep(p, &p->lock)，等待子进程退出时的唤醒。

#### 返回值

- 成功时返回子进程的 PID。
- 失败时（无子进程、复制失败或进程被杀死）返回 -1。

#### 关键点

- 父进程通过 wait 回收僵尸子进程，防止资源泄漏。
- 使用锁避免竞争条件，确保不会错过子进程的退出。

---

### kill 函数

```
// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}
```
这个函数向指定 PID 的进程发送终止信号。目标进程不会立即退出，而是在下次返回用户空间时处理（例如在 usertrap 中）。

#### 工作流程

1. **遍历进程表**：
    - 从 proc 到 proc[NPROC]，查找目标进程。
2. **检查并标记进程**：
    - acquire(&p->lock)：锁定当前遍历到的进程。
    - 如果 p->pid == pid：
        - p->killed = 1：标记进程为待杀死。
        - 如果进程在睡眠状态（p->state == SLEEPING），将其状态改为 RUNNABLE，唤醒它以处理终止。
        - 释放锁，返回 0 表示成功。
    - 否则释放锁，继续查找。
3. **未找到目标进程**：
    - 如果遍历完仍未找到，返回 -1。

#### 返回值

- 成功时返回 0。
- 失败时（未找到指定 PID）返回 -1。

#### 关键点

- kill 仅标记进程，不会直接终止，实际退出由进程的陷阱处理逻辑完成。
- 如果进程在睡眠，唤醒它以加快终止。

---

### 总结

- **exit**：清理资源，将进程设为僵尸状态，跳转到调度器。
- **wait**：等待并回收子进程，获取其退出状态。
- **kill**：标记目标进程为待杀死，唤醒睡眠中的进程。

这些函数共同实现了进程生命周期的管理，包括退出、等待和终止，是操作系统中进程控制的基础。

---
### 1. 为什么要 reparent？

在 exit 函数中，reparent(p) 的作用是将当前进程的所有子进程重新分配给另一个进程（通常是 initproc）。这是操作系统进程管理中的一个重要机制，原因如下：

#### 背景

- 在类 Unix 系统中，每个进程都有一个父进程，父进程负责通过 wait 回收子进程的资源（例如僵尸进程）。
- 当一个进程退出时，如果它有子进程，这些子进程会变成“孤儿”（orphaned），因为它们的父进程已经不存在。

#### 问题

- 如果不重新分配这些子进程，它们将没有父进程来调用 wait，导致这些子进程在退出后无法被回收，变成“僵尸进程”，浪费系统资源。

#### 解决方案

- reparent(p) 将当前进程的子进程的父指针（np->parent）重新指向 initproc（初始进程，通常 PID 为 1）。
- 这样，initproc 成为这些子进程的新父进程，负责在它们退出时调用 wait 回收资源。

```
// we might re-parent a child to init. we can't be precise about
  // waking up init, since we can't acquire its lock once we've
  // acquired any other proc lock. so wake up init whether that's
  // necessary or not. init may miss this wakeup, but that seems
  // harmless.
  acquire(&initproc->lock);
  wakeup1(initproc);
  release(&initproc->lock);

  // grab a copy of p->parent, to ensure that we unlock the same
  // parent we locked. in case our parent gives us away to init while
  // we're waiting for the parent lock. we may then race with an
  // exiting parent, but the result will be a harmless spurious wakeup
  // to a dead or wrong process; proc structs are never re-allocated
  // as anything else.
  acquire(&p->lock);
  struct proc *original_parent = p->parent;
  release(&p->lock);

  // we need the parent's lock in order to wake it up from wait().
  // the parent-then-child rule says we have to lock it first.
  acquire(&original_parent->lock);

  acquire(&p->lock);

  // Give any children to init.
  reparent(p);
```

- reparent(p) 执行重新分配。
- 唤醒 initproc，让它检查是否有新的子进程需要回收。

#### 为什么选择 initproc？

- initproc 是系统中第一个用户进程（通常是 init，PID 为 1），它是一个特殊的、永远运行的进程，负责管理系统中的孤儿进程。这是 Unix 系统的传统设计。
- ---
### 2. initproc 有什么用？

initproc 是操作系统中的初始进程，通常由内核在启动时创建（在 xv6 中，它是第一个用户进程）。它的作用包括：

#### 主要职责

1. **孤儿进程的“收养者”**：
    - 如上所述，当普通进程退出时，initproc 接管其子进程，确保它们在退出后能被正确回收。
    - initproc 会持续运行并调用 wait，清理这些孤儿进程的僵尸状态。
2. **系统启动的起点**：
    - 在系统启动时，内核创建 initproc 作为用户空间的第一个进程。它通常会启动其他系统服务或 shell，成为系统的根进程。
3. **永不退出**：
    - initproc 不能退出（exit 中有 if(p == initproc) panic("init exiting")），因为它是系统正常运行的保障。如果它退出，系统将无法处理孤儿进程，可能导致崩溃。

#### 在 exit 中的作用

- 当一个进程退出时，initproc 被唤醒（wakeup1(initproc)），以便检查是否有新分配给它的子进程需要处理。
- 这种设计确保系统中始终有一个可靠的进程来管理孤儿进程。

---

### 3. 为什么 sched 返回就 panic？

在 exit 函数的最后，调用了 sched()，并在其后紧跟 panic("zombie exit")

这背后的原因与进程状态和调度器的设计有关。

#### sched 的作用

- sched() 是调度器函数，它将当前进程的控制权交给操作系统，让调度器选择下一个要运行的进程。
- 在调用 sched() 时，当前进程已经完成了退出流程，状态被设置为 ZOMBIE，并且释放了所有必要的锁（例如 original_parent->lock）。

#### 为什么不应该返回

- **逻辑上**：一个退出中的进程（状态为 ZOMBIE）已经完成了它的生命周期，不应该再次运行。它的资源已经被清理（文件关闭、目录释放等），CPU 不应该再调度它执行代码。
- **设计上**：sched() 被设计为将控制权永久移交出去。退出进程的任务是进入僵尸状态，等待父进程回收，而不是恢复执行。
- **实现上**：在正常的调度流程中，sched() 会切换到另一个进程的上下文，而当前进程的上下文（栈、寄存器等）可能已经被修改或废弃。如果 sched() 返回，说明调度器出了问题，当前进程被错误地重新调度。

#### panic 的意义

- 如果 sched() 返回，说明调度器未能正确切换到其他进程，这是一个严重的逻辑错误或实现缺陷。
- 调用 panic("zombie exit") 会终止系统并输出错误信息，提醒开发者调度器出现了异常（例如，进程状态管理错误或上下文切换失败）。
- 在正常情况下，sched() 永远不会返回到 exit 函数，因为进程已经被移出运行队列。

#### 可能的异常场景

- 如果系统中没有其他可运行的进程（例如所有进程都已退出），调度器可能无法找到下一个进程。但在设计良好的系统中，initproc 应该始终存在，避免这种情况。因此，sched 返回几乎总是意味着 bug。

---

### 综合回答

1. **为什么要 reparent**？
    - 为了避免子进程成为孤儿，确保它们在退出后能被回收。reparent 将子进程交给 initproc，保持系统资源管理的完整性。
2. **什么是 initproc 及其作用**？
    - initproc 是初始进程（PID 通常为 1），负责收养孤儿进程并回收它们的资源，同时是系统启动的根进程。它永不退出，是系统稳定的基石。
3. **为什么 sched 返回就 panic**？
    - sched() 是退出进程的最后一步，正常情况下它不会返回。如果返回，说明调度器错误地重新调度了一个僵尸进程，这是严重的系统错误，因此触发 panic。

---
### 为什么在 exit 中说它是“最后一步”且不应该返回，而在时钟中断中却可以返回到 yield？

### 背景：sched 的两种使用场景

sched 函数是 xv6（或其他类似操作系统）中调度器的核心入口，它负责将当前进程的控制权交给调度器，让系统选择下一个要运行的进程。它的行为取决于调用它的上下文：

1. **正常调度（如时钟中断中的 yield）**：进程主动让出 CPU，之后可能被重新调度。
2. **进程退出（如 exit 中的调用）**：进程结束生命周期，不应再被调度。

### 1. 时钟中断中的 sched 和 yield

在时钟中断的场景中，sched 的调用通常通过 yield 函数发生。让我们看看这个流程：

#### 时钟中断的典型流程

- **时钟中断触发**：硬件定时器触发中断，进入内核的陷阱处理（trap.c 中的 usertrap 或 kerneltrap）。
- **检查时间片**：如果当前进程的时间片用尽（例如 ticks 达到某个阈值），内核决定让进程让出 CPU。
- **调用 yield**：
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
    
- **sched 的行为**：
    - 保存当前进程的上下文（寄存器、栈指针等）。
    - 将 CPU 切换到调度器（scheduler 函数），选择下一个 RUNNABLE 状态的进程。
    - 如果当前进程仍然是 RUNNABLE，它可能在未来被重新调度。
- **返回**：当调度器再次选择这个进程运行时，sched 的上下文会被恢复，控制流回到 yield 中，yield 再返回到调用它的地方（通常是陷阱处理代码）。

#### 为什么这里 sched 可以返回？

- 在这个场景中，进程只是暂时让出 CPU，状态被设为 RUNNABLE，表示它仍然是活跃的，可以在未来被调度器选中。
- sched 的返回意味着进程被重新调度，恢复到之前的中断点继续执行。这是正常的调度行为。

---

### 2. exit 中的 sched

在 exit 函数中，sched 的调用发生在进程退出流程的最后：

```
void exit(int status) {
  // ... 清理资源、reparent 等 ...
  p->xstate = status;
  p->state = ZOMBIE;  // 标记为僵尸状态
  release(&original_parent->lock);
  sched();            // 进入调度器
  panic("zombie exit");
}
```

#### sched 的行为

- **状态变化**：在调用 sched 之前，进程状态被设为 ZOMBIE，表示它已经退出，不再是可运行的。
- **调度器逻辑**：调度器（scheduler 函数）会检查所有进程的状态，只选择 RUNNABLE 的进程运行。ZOMBIE 状态的进程应该被跳过。
- **预期结果**：sched 将当前进程的控制权交给调度器，调度器选择一个新的 RUNNABLE 进程运行，当前进程的上下文被丢弃，不再返回。

#### 为什么说它是“最后一步”且不应该返回？

- **“最后一步”的含义**：对于退出中的进程，sched 是它在 CPU 上执行的最后动作。之后，它***进入 ZOMBIE 状态，等待父进程通过 wait 回收，不再参与调度***。

- **不应该返回的理由**：
    - 进程状态已经是 ZOMBIE，调度器不应该重新选择它运行。
    - 它的资源（文件、目录等）已经被清理，上下文可能已经无效。如果返回，进程会尝试执行未定义的行为，导致系统崩溃或数据损坏。
- **如果返回怎么办**？
    - 如果 sched 返回，说明调度器错误地将一个 ZOMBIE 进程重新调度。这可能是调度器逻辑的 bug（例如未正确检查进程状态），或者进程状态管理出现问题。
    - 因此，panic("zombie exit") 被用作安全检查，立即终止系统并报告错误。

---

### 3. 为什么两种场景的 sched 行为不同？

关键区别在于**进程状态**和**调用意图**：

- **时钟中断中的 sched**：
    - 进程状态：RUNNABLE。
    - 意图：暂时让出 CPU，进程仍然活跃，未来可能被重新调度。
    - 返回：正常行为，进程恢复执行。
- **exit 中的 sched**：
    - 进程状态：ZOMBIE。
    - 意图：永久退出，进程生命周期结束，不再运行。
    - 返回：异常行为，表示调度器出错。

### 总结

- **时钟中断中的 sched**：进程是 RUNNABLE，让出 CPU 后可以返回，属于正常调度。
- **exit 中的 sched**：进程是 ZOMBIE，生命周期结束，sched 是最后一步，不应返回。返回表示调度器 bug，因此 panic。
- **区别根源**：进程状态和调用目的决定了 sched 的行为。

---
### 父进程先退出而子进程还未退出的情况

#### 解决方法：重新分配父进程（Reparenting）

类 Unix 系统中通过 **将孤儿进程的父进程重新分配给 init 进程** 来解决这个问题。这正是 exit 函数中 reparent 的作用。
#### 机制

- 当父进程调用 exit 时：
    - 系统会检查该进程是否有子进程。
    - 如果有子进程，将这些子进程的父指针（proc->parent）重新指向 init 进程（通常是 PID 为 1 的进程）。
- init 进程是一个特殊的进程，它永远运行，并且负责回收所有孤儿进程的资源。

```
时间点       进程状态
t0:          PID 10 (RUNNING) -> fork -> PID 11 (RUNNING)
t1:          PID 10 调用 exit -> ZOMBIE, PID 11 的父进程改为 PID 1
t2:          PID 11 (RUNNING), PID 10 (ZOMBIE)
t3:          PID 11 调用 exit -> ZOMBIE
t4:          PID 1 (init) 调用 wait -> 回收 PID 11
```

---
