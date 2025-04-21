## 重点总结

1. **锁与中断的冲突**：
    - 线程和中断共享数据（如ticks），锁（如tickslock）保护访问。
2. **死锁风险**：
    - 中断尝试获取线程持有的锁，同一CPU上形成死锁。
3. **xv6策略**：
    - 获取锁时禁用中断（push_off()），释放锁后恢复（pop_off()）。
4. **顺序关键**：
    - acquire()：***先禁用中断再设锁，避免窗口期死锁***。
    - release()：先释放锁再恢复中断，确保安全。
5. **嵌套管理**：
    - ***noff和intena跟踪锁层级，正确恢复中断状态***。
6. **效果**：
    - 单CPU避免死锁，多CPU保持并行性。

---

### 1. 锁与中断处理函数的背景

#### 共享数据的并发访问

在xv6中，内核线程和中断处理程序可能同时访问共享数据。例如：

- **场景**：ticks变量（全局时钟计数）。
    - 内核线程（如sys_sleep）读取ticks以计算睡眠时间。
    - 定时器中断处理程序（如clockintr）增加ticks。
- **问题**：如果没有保护，线程读取时中断可能修改ticks，导致不一致。

#### 使用锁保护

- **解决方法**：tickslock自旋锁串行化对ticks的访问。
- **代码**：
    - sys_sleep（kernel/sysproc.c）：  
```
acquire(&tickslock);
uint t0 = ticks;
while(ticks - t0 < n) {
  sleep(&ticks, &tickslock);
}
release(&tickslock);
```
        
- clockintr（kernel/trap.c）：  
        
```
acquire(&tickslock);
ticks++;
wakeup(&ticks);
release(&tickslock);    
```

---

### 2. 锁与中断的潜在危险：死锁

#### 死锁场景

假设以下情况：

1. CPU0上的sys_sleep持有tickslock，正在读取ticks。
2. 定时器中断触发，调用clockintr。
3. clockintr尝试acquire(&tickslock)，但锁被sys_sleep持有，进入自旋等待。
4. ***sys_sleep无法继续运行（因为中断未返回）***，无法释放tickslock。

- **结果**：CPU0死锁，系统冻结。

#### 原因

- 自旋锁的特性：等待锁时CPU忙等待，不让出控制权。
- 中断上下文：中断处理程序运行在当前线程的栈上，打断线程执行。

#### 类比：会议室钥匙

- **场景**：会议室（ticks）有一把钥匙（tickslock）。
- **线程**：经理（sys_sleep）拿钥匙进会议室开会。
- **中断**：保安（clockintr）突然进来要锁门，但钥匙在经理手里。
- **死锁**：保安等着经理交钥匙，经理等着保安离开才能继续，双方僵持。

---

### 3. xv6的解决策略

#### 核心原则

- **避免中断持有锁**：***如果锁被中断处理程序使用，CPU在启用中断时不能持有该锁***。

- **xv6的保守策略**：获取任何锁时，禁用当前CPU的中断。

#### 实现细节

1. **acquire()禁用中断**：
    - **代码**（kernel/spinlock.c）：  
```
void acquire(struct spinlock *lk) {
  push_off();  // 禁用中断
  if(holding(lk))
    panic("acquire");
  while(__sync_lock_test_and_set(&lk->locked, 1) != 0)
    ;  // 自旋等待
  lk->cpu = mycpu();
}
```
        
 **push_off()**：关闭中断并记录嵌套级别。

2. **release()恢复中断**：
    - **代码**：
```
void release(struct spinlock *lk) {
  lk->cpu = 0;
  __sync_synchronize();
  lk->locked = 0;
  pop_off();  // 恢复中断
}
```
        
**pop_off()**：减少嵌套计数，***计数为0时恢复中断***。

3. **嵌套锁管理**：
    - **push_off()和pop_off()**：  
```
void push_off(void) {
  int old = intr_get();
  intr_off();
  if(mycpu()->noff == 0)
    mycpu()->intena = old;
  mycpu()->noff += 1;
}

void pop_off(void) {
  if(mycpu()->noff < 1)
    panic("pop_off");
  mycpu()->noff -= 1;
  if(mycpu()->noff == 0 && mycpu()->intena)
    intr_on();
}
```
        
**noff**：记录锁嵌套级别。
**intena**：***保存最外层中断状态，嵌套清零时恢复***。

#### 为什么禁用中断？

- **单CPU安全**：防止同一CPU上线程和中断处理程序竞争锁。
- **多CPU兼容**：其他CPU的中断仍可运行，等待锁释放，不影响整体系统。

#### 类比：会议室改进

- **新规则**：经理（线程）拿钥匙时关门（禁用中断），保安（中断）进不来。
- **结果**：保安在门外等，经理开完会放钥匙（释放锁），无死锁。

---

### 4. 顺序的重要性

#### acquire()中先push_off()再设锁

- **代码**：
```
push_off();
while(__sync_lock_test_and_set(&lk->locked, 1) != 0)
  ;
```
    
- **原因**：
    - 先禁用中断，确保中断不会在锁持有期间触发。
    - 如果顺序颠倒（先设锁再禁用中断），存在**窗口期**：
        - 设锁后、禁用中断前，定时器中断触发，clockintr尝试获取锁，死锁。
- **类比**：锁门前关警报（中断），避免警报响时门已锁。

#### release()中先释放锁再pop_off()

- **代码**：
```
lk->locked = 0;
pop_off();
```
- **原因**：
    - 先释放锁，确保其他CPU或中断可获取。
    - 后恢复中断，避免释放前中断触发竞争。
- **类比**：**开门后开警报**，确保别人能进门。

---

### 5. 示例分析

#### sys_sleep和clockintr  

- **流程**：
    1. sys_sleep调用acquire(&tickslock)，禁用中断。  
    2. 定时器中断触发，但因中断关闭，clockintr推迟。
    3. sys_sleep释放tickslock，启用中断。
    4. clockintr运行，安全获取tickslock。
- **结果**：无死锁，访问串行化。

#### 类比：火车调度

- **火车（线程）**：持有轨道锁（tickslock），关闭信号（中断）。
- **检查员（中断）**：想检查轨道，但信号关，等火车走。
- **顺序**：火车走后开信号，检查员再上。

---


### 类比总结：锁如门禁系统

- **场景**：大楼（内核）有门禁卡（锁）和警卫（中断）。
- **无策略**：员工（线程）拿卡时警卫进来抢卡，死锁。
- **xv6方案**：员工拿卡时关警卫通道（禁用中断），离开后开通道（恢复中断）。
- **顺序**：先关通道再拿卡，先放卡再开通道，避免冲突。