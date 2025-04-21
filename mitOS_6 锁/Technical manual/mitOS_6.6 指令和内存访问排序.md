
## 重点总结

1. **重新排序原因**：
    - 编译器和CPU为性能优化（如流水线、隐藏延迟）重新排序指令。
2. **并发问题**：
    - 弱内存模型下，重新排序可能导致其他CPU看到未完成状态（如list->next未初始化）。
3. **内存屏障**：
    - *__sync_synchronize()强制内存操作顺序，防止跨锁重新排序。*
    - acquire：保护前置初始化；release：确保更新可见。
4. **xv6实现**：
    - 在***锁操作中插入屏障***，保证临界区正确性。
5. **关键点**：
    - 单线程无影响，多线程需显式同步。
---

### 1. 指令和内存访问排序的基本概念

**自然假设**：程序员通常认为代码按源代码顺序执行，即一条语句完成后执行下一条。然而，为了优化性能，编译器和CPU可能重新排序指令或内存访问。

#### 为什么重新排序？

- **编译器优化**：
    - 编译器可能调整指令顺序，以减少等待时间或提高指令流水线效率。
- **CPU乱序执行**：
    - 现代CPU使用乱序执行（out-of-order execution）和投机执行（speculative execution），提前执行后续指令，隐藏延迟（如内存访问的等待时间）。
- **例子**：
    - 指令A需要访问内存（慢），指令B是简单计算（快）。
    - CPU可能先执行B，再等A完成。

#### 重新排序的前提

- **单线程正确性**：重新排序不会改变串行程序的结果。
- **依赖性**：如果两条指令无数据依赖（即不互相读写同一变量），可以交换顺序。

---

### 2. 并发中的问题

在多核或多线程环境下，重新排序可能破坏并发代码的正确性，因为其他CPU可能看到未预期的中间状态。

#### 文中例子：push()代码

```
struct element *list = 0;
struct lock listlock;

void push(int data) {
    struct element *l;
    l = malloc(sizeof *l);    // 第1行
    l->data = data;           // 第2行
    acquire(&listlock);       // 第3行
    l->next = list;           // 第4行
    list = l;                 // 第5行
    release(&listlock);       // 第6行
}
```

- **预期**：
    - 锁保护临界区（第4-5行），其他CPU只能在release后看到更新后的list。
- **重新排序问题**：
    - 假设编译器或CPU将第4行（l->next = list）推迟到第6行（release）之后：
```
acquire(&listlock);
list = l;              // list指向新元素
release(&listlock);
l->next = list;        // 太晚了！
```
      
- **后果**：
    - 在release后，另一个CPU获取锁，看到list = l，但l->next尚未初始化（可能是垃圾值）。
    - 链表不一致，违反了不变量（list应指向完整初始化的元素）。

#### 为什么会出错？

- **内存模型**：
    - CPU的内存模型定义了指令和内存访问的可见顺序。
    - 弱内存模型（如RISC-V的默认模型）允许load和store重新排序，只要单线程结果不变。
- **并发可见性**：
    - 其他CPU可能在锁释放后立即访问list，看到未完成的状态。

---

### 3. 内存屏障的作用

**定义**：***内存屏障（memory barrier）是一种指令，强制编译器和CPU按特定顺序执行内存操作，防止跨屏障的重新排序***。

#### xv6中的实现

- **__sync_synchronize()**：
    - 在acquire()和release()中使用。
    - 对应***RISC-V的fence指令，确保屏障前的内存操作完成后，才执行屏障后的操作***。
- **效果**：
    - acquire()：确保临界区前的初始化（如l->data）在锁获取前完成。
    - release()：确保临界区内的更新（如list = l）在锁释放前对其他CPU可见。

#### 修改后的push()

```
void push(int data) {
    struct element *l;
    l = malloc(sizeof *l);
    l->data = data;
    acquire(&listlock);       // 含__sync_synchronize()
    l->next = list;
    list = l;
    release(&listlock);       // 含__sync_synchronize()
}
```

- **屏障作用**：
    - acquire前的操作（如l->data = data）不会推迟到锁内。
    - release后的操作不会提前到锁内更新完成前。

---

### 4. 类比与举例

#### 类比：厨房流水线

- **场景**：
    - 厨师A准备三明治：1. 拿面包，2. 涂黄油，3. 上锁，4. 加火腿，5. 解锁。
    - 预期：锁保护火腿添加，其他厨师只能在解锁后拿完整三明治。
- **无屏障**：
    - 厨师A乱序：拿面包→上锁→解锁→涂黄油→加火腿。
    - 厨师B在解锁后拿走三明治，只有面包，缺少黄油和火腿。
- **加屏障**：
    - 锁前屏障：确保面包和黄油在锁前完成。
    - 锁后屏障：确保火腿在解锁前添加。
    - 结果：B拿到完整三明治。

#### 例子：共享计数器

- **代码**：
```
int counter = 0;
struct spinlock lock;

void increment() {
    int temp;
    acquire(&lock);
    temp = counter;
    counter = temp + 1;
    release(&lock);
}
```
- **无屏障问题**：
    - CPU将counter = temp + 1推迟到release后。
    - 另一个CPU在release后看到counter未更新。
- **加屏障**：
    - release中的__sync_synchronize()确保counter更新在锁释放前完成。

---

### 5. 内存模型与xv6

- **弱内存模型**：
    - ***RISC-V允许load/store重新排序，除非显式屏障***。
    - 对比强内存模型（如x86-TSO），默认顺序更严格。
- **xv6策略**：
    - 在acquire和release中插入屏障，强制顺序。
    - 适用于锁保护的共享数据访问。
- **例外**：
    - 第9章可能讨论无锁编程或特殊情况，需更复杂的同步。
