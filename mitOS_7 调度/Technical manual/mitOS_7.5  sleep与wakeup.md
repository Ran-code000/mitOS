### 重点总结

1. **sleep 和 wakeup**：
    - 进程休眠等待事件，事件发生后唤醒，协调交互。
2. **忙等问题**：
    - 初始信号量浪费 CPU，需优化。
3. **丢失唤醒**：
    - *检查和睡眠非原子，V 唤醒早于 P 睡眠，通知丢失*。
4. **错误锁方案**：
    - 检查前持锁避免丢失，但睡眠持锁导致死锁。
5. **正确方案**：
    - ***sleep 带条件锁，睡眠后释放，醒来重获***，解决两者。
6. **原子性**：
    - 睡眠和释放锁需原子，保护状态转换。

---

### 在 xv6 中的体现

- **代码**：
    - kernel/proc.c：sleep 和 wakeup。
    - sleep(chan, lk)：传入锁，原子操作。
- **实现**：
    - p->state = SLEEPING，释放 lk，加回 p->lock
---

#### 1. **sleep 和 wakeup 的目的**

- **描述**：
    - sleep 让进程等待事件时休眠，释放 CPU；wakeup 在事件发生后唤醒进程，称为序列协调（sequence coordination）或条件同步机制（conditional synchronization mechanisms）。
- **类比**：
    - 想象一个面包店，顾客（进程）等面包（事件）。顾客告诉店员（sleep）“没面包我先睡”，店员腾出座位（释放 CPU）；面包师烤好面包（事件），喊醒顾客（wakeup）。
- **解释**：
    - 提供进程间协作，避免忙等。

#### 2. **信号量与忙等问题**

错误版本：
```
struct semaphore {
    struct spinlock lock;
    int count;
};

void V(struct semaphore* s) {
    acquire(&s->lock);
    s->count += 1;
    release(&s->lock);
}

void P(struct semaphore* s) {
    while (s->count == 0)
        ;
    acquire(&s->lock);
    s->count -= 1;
    release(&s->lock);
}

```
- **描述**：
    - 初始信号量用忙等（while 循环），消费者浪费 CPU 检查 count。
- **类比**：
    - 顾客不停问“面包好了吗”（忙等），占着座位浪费时间，店员很忙（CPU 浪费）。
- **解释**：
    - 忙等低效，需让出 CPU。

#### 3. **引入 sleep 和 wakeup**


- **描述**：
    - sleep(chan) 在通道上休眠，wakeup(chan) 唤醒等待进程。
- **类比**：
    - 顾客说“没面包我在沙发睡”（sleep），面包师喊“面包好了”（wakeup），顾客醒来。
- **解释**：
    - 替换忙等，优化资源。

#### 4. **丢失唤醒问题**
错误版本：
```
void V(struct semaphore* s) {
    acquire(&s->lock);
    s->count += 1;
    wakeup(s);  // !pay attention
    release(&s->lock);
}

void P(struct semaphore* s) {
    while (s->count == 0)
        sleep(s);  // !pay attention
    acquire(&s->lock);
    s->count -= 1;
    release(&s->lock);
}

```
- **描述**：
    - P 检查 count == 0 后，V 增加 count 并调用 wakeup，但 P 未睡，wakeup 无效果，P 再睡，错过通知。
- **类比**：
    - 顾客检查没面包（count == 0），走向沙发时，面包师喊“面包好了”（wakeup），但顾客没睡，喊声无效。顾客睡下后，面包师不再喊，顾客永远等。
- **解释**：
    - 检查和睡眠非原子，导致通知丢失。

#### 5. **错误修复：锁提前**
```
void V(struct semaphore* s) {
    acquire(&s->lock);
    s->count += 1;
    wakeup(s);
    release(&s->lock);
}

void P(struct semaphore* s) {
    acquire(&s->lock);  // !pay attention
    while (s->count == 0)
        sleep(s);
    s->count -= 1;
    release(&s->lock);
}

```
- **描述**：
    - 将 acquire 移到检查前，使检查和 sleep 原子，但 P 睡时持锁，V 死锁。
- **类比**：
    - 顾客锁门（acquire）检查面包，睡沙发（sleep），锁住门。面包师拿面包来（V），敲不开门（死锁）。
- **解释**：
    - 锁保护检查，但睡眠持锁阻塞生产者。

#### 6. **正确方案：sleep 带锁**

```
void V(struct semaphore* s) {
    acquire(&s->lock);
    s->count += 1;
    wakeup(s);
    release(&s->lock);
}

void P(struct semaphore* s) {
    acquire(&s->lock);

    while (s->count == 0)
        sleep(s, &s->lock);  // !pay attention
    s->count -= 1;
    release(&s->lock);
}
```
- **描述**：
    - sleep(chan, lock) 传入条件锁，睡眠后释放锁，醒来重获锁，避免丢失唤醒和死锁。
- **类比**：
    - 顾客锁门检查面包，睡沙发时交给店员钥匙（释放锁），面包师进来喊醒（wakeup），顾客醒后拿回钥匙（重获锁）。
- **解释**：
    - ***锁保护检查到睡眠的原子性，释放避免死锁***。

#### 7. **原子性要求**

- **描述**：
    - sleep 标记睡眠和释放锁需原子。
- **类比**：
    - 顾客睡下和交钥匙一气呵成，面包师不能在交钥匙前闯入。
- **解释**：
    - 防止状态转换中被打断。

---

### 类比举例：生产者-消费者

- **场景**：
    - 面包师（V）烤面包，顾客（P）取面包，信号量 s->count 表示面包数。
- **错误版本（丢失唤醒）**：
    1. 顾客检查没面包（count == 0），走向沙发。
    2. 面包师放面包（count = 1），喊“好了”（wakeup），无人睡，无效。
    3. 顾客睡下（sleep），永远等。
- **错误版本（死锁）**：
    1. 顾客锁门检查，睡沙发，持锁。
    2. 面包师敲门（acquire），进不去，死锁。
- **正确版本**：
    1. 顾客锁门检查，睡沙发交钥匙（sleep(s, &s->lock)）。
    2. 面包师开门放面包，喊醒（wakeup）。
    3. 顾客醒来拿钥匙，取面包（count -= 1）。
---
详见 mitOS_7.1 多路复用 文末

```

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.
  if(lk != &p->lock){  //DOC: sleeplock0
    acquire(&p->lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &p->lock){
    release(&p->lock);
    acquire(lk);
  }
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == SLEEPING && p->chan == chan) {
      p->state = RUNNABLE;
    }
    release(&p->lock);
  }
}
```

---
#### 区别总结

|特性|信号量锁 (s->lock)|进程锁 (p->lock)|
|---|---|---|
|**保护对象**|信号量的 count 等字段|进程的 state、chan 等字段|
|**使用范围**|信号量操作（P 和 V）|进程管理和调度|
|**持有者**|调用 P 或 V 的进程|调度相关代码（如 sleep、scheduler）|
|**目的**|同步生产者与消费者|保护进程元数据的完整性|
从消费者获取信号量锁到最终重新获取信号量锁的整个过程

1. **消费者获得信号量锁并将信号量锁传入 sleep** ： acquire(&s->lock)；sleep(s, &s->lock)
2. **sleep 释放信号量锁** ： release(&s->lock)。
3. **生产者获得信号量锁** ： acquire(&s->lock) 在 V 中。
4. **执行完 wakeup 后生产者释放信号量锁** ： release(&s->lock) 在 V 中。
5. **完成调度后回到 sleep 后 sleep 获得信号量锁** ： 从 sched() 返回，acquire(&s->lock) 
6. **消费者执行 P 中的后续代码并释放信号量锁** ： s->count -= 1 和 release(&s->lock)

进程锁在整个过程中的获取和释放流程

1. **开始不持有进程锁**
2. **消费者在 sleep 中获得进程锁**：acquire(&p->lock)
3. **进入 scheduler**：此时不释放 p->lock，锁仍被持有
4. **生产者调用 wakeup**：依赖于 s->lock 已释放。
5. **wakeup 获得进程锁更改状态释放进程锁**
6. **scheduler 中获得进程锁，切换上下文执行新进程**
7. **切换上下文返回到 sleep 中，释放进程锁**：release(&p->lock) 在 sleep 返回时执行。


==疑惑：**如果 p1->lock 已经被持有，scheduler 如何再次获取它？自旋锁不是只能被一个线程/CPU 持有吗？还能再获得吗？**==

#### 自旋锁的基本规则

- 自旋锁（spinlock）的设计通常是互斥的：一旦被某个 CPU/线程持有，其他尝试获取它的 CPU/线程会自旋（忙等）直到锁被释放。
- 在单线程或单 CPU 上下文中，同一个线程试图再次获取自己持有的锁会导致死锁（如果没有特殊机制如可重入锁）。

但在 scheduler 的场景中，情况有所不同，我们需要结合上下文切换和锁的持有者来分析

 ==**为什么 scheduler 可以再次获取 p1->lock？**==

- **关键点**：scheduler 运行在同一个 CPU（CPU0）上，且是在 p1 的上下文切换之后。
- **锁的持有者**：
    - ***自旋锁的“持有”状态是由 CPU 和执行上下文决定的***。在 sleep 中，p1->lock 被 CPU0 持有，但***当 swtch 切换到 scheduler 时，CPU0 的执行流变为调度器代码***。
    - scheduler 中的 acquire(&p1->lock) 是由同一个 CPU0 执行的，而此时 ***p1 的上下文已经暂停，不再“活跃”地持有锁***

**进入 scheduler 时 p1->lock 的状态**

- 在 sleep 中：
    - acquire(&p1->lock)：消费者进程 p1 在当前 CPU（假设为 CPU0）上持有 p1->lock。
    - sched() 检查 p1->lock 被持有（if(!holding(&p->lock)) panic），然后执行 swtch(&p1->context, &c->context)。
- **上下文切换**：
    - swtch 保存 p1 的寄存器状态（包括程序计数器 PC），切换到 CPU 的调度器上下文（c->context）。
    - 此时，p1 的执行被暂停，CPU0 开始运行 scheduler。
- **锁状态**：
    - ***p1->lock 在技术上仍被标记为“被持有”（locked 字段为 1），但持有者是 p1 的上下文，而 p1 已经暂停***。
    - ***CPU0 现在运行的是 scheduler，不再直接代表 p1 的执行流***。


**当 scheduler 执行 acquire(&p1->lock) 时：**

- 如果锁仍被标记为“被持有”（locked == 1），但**当前 CPU 是之前持有它的 CPU（CPU0）**，锁的检查逻辑允许继续执行（因为没有其他 CPU 竞争）。
- ***如果有其他 CPU 尝试获取 p1->lock，会自旋等待***，但这里是单 CPU 场景，scheduler 不会被其他 CPU 干扰。


==**为什么不会死锁？**==
- **单 CPU 场景**：
    - 在单核系统中，scheduler 和 p1 的执行是串行的。p1 暂停后，scheduler 接管 CPU0，锁的“持有”状态在上下文切换后由 scheduler 继承。
    - acquire(&p1->lock) 在这种情况下不会自旋，因为没有其他竞争者。
- **多 CPU 场景**：
    - 如果生产者运行在另一个 CPU（比如 CPU1），它通过 wakeup 获取并释放 p1->lock，这发生在 scheduler 遍历到 p1 之前。
    - 当 CPU0 的 scheduler 到达 acquire(&p1->lock) 时，锁已经被 wakeup 释放（release(&p1->lock)），因此可以成功获取。
- **锁的释放时机**：
    - p1->lock 的最终释放发生在 sleep 返回时（release(&p1->lock)），而不是在进入 scheduler 时。


---

### 时序调整

- **多核竞争**：wakeup 和 scheduler 可能同时尝试 acquire(&p1->lock)。
    - 如果 scheduler 先获取，wakeup 等待，scheduler 释放后 wakeup 执行。
    - 如果 wakeup 先获取，scheduler 等待，wakeup 释放后 scheduler 执行。
- **假设顺序**：为简化，假设 scheduler 先遍历到 p1（SLEEPING），释放 p1->lock，然后 wakeup 修改状态，scheduler 再次遍历到 p1（RUNNABLE）。

### 完整流程总结

1. **消费者获取信号量锁并传入 sleep**：
    - s->lock：（CPU0）
    - p1->lock：未持有
2. **sleep 获取进程锁，释放信号量锁**：
    - s->lock：未持有（CPU0 已释放）
    - p1->lock：（CPU0）
3. **进入 scheduler，进程锁未释放**：
    - s->lock：未持有
    - p1->lock：（CPU0）
4. **生产者获取信号量锁**：
    - s->lock：（CPU1）
    - p1->lock：（CPU0）
5. **wakeup 获取进程锁，等待 CPU0 释放进程锁**：
    - s->lock：（CPU1）
    - p1->lock：未持有（CPU1，等待 CPU0 释放）
6. **scheduler 遍历到 p1（SLEEPING），释放 p1->lock**：
	- s->lock：（CPU1）
    - p1->lock：未持有（CPU0 已释放）
7.  **wakeup 获取进程锁**：
	- s->lock：（CPU1）
    - p1->lock：（CPU1）
8. **wakeup 修改状态释放进程锁，生产者释放信号量锁**：
    - s->lock：未持有（CPU1 已释放）
    - p1->lock：未持有（CPU1 已释放）
9. **secheduler 获取进程锁，切换上下文**：
    - s->lock：未持有。
    - p1->lock：（CPU0）
10. **sleep 返回，释放进程锁，获取信号量锁**：
    - s->lock：（CPU0）
    - p1->lock：未持有（CPU0 已释放）
11. **消费者完成 P，释放信号量锁**：
    - s->lock： 未持有（CPU0 已释放）
    - p1->lock：未持有
---
- **调用sleep时需要持有condition lock，这样sleep函数才能知道相应的锁，防止还未进入sleep就wakeup 就被调用了**。
- **sleep函数只有在获取到进程的锁p->lock之后，才能释放condition lock，避免窗口时间中 lose wakeup**。
- **wakeup需要同时持有两个锁才能查看进程，条件锁使得 wakeup 能被调用，进程锁使得 wakeup 有权限查看进程状态**。
