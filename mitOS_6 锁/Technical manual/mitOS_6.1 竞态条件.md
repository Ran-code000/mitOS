### 重点总结

1. **竞态条件**：
    - 多进程并发读写共享数据（如list），结果依赖时序，可能导致丢失更新或不一致。
    - 示例：无锁push()丢失元素，kfree()丢失页面。
2. **锁的作用**：
    - 提供***互斥，串行化临界区***，保护不变量（如链表结构）。
    - ***确保操作原子性，避免部分更新可见***。
3. **锁的代价**：
    - 串行化降低并发性，锁争用影响性能。
    - 优化：细粒度锁、独立数据结构（如每CPU空闲列表）。
4. **设计原则**：
    - 最小化临界区，保护必要数据，权衡正确性与性能。
---

### 1. 什么是竞态条件？

**定义**：竞态条件是指多个进程（或线程）并发访问共享数据，其中至少有一个是写操作，且结果依赖于执行的时序。如果没有适当的同步机制，这种并发可能导致数据不一致或错误。

**例子**：文中提到两个父进程在不同CPU上调用wait()，释放子进程的内存页，通过kfree()将页面放回空闲列表。如果kfree()的实现没有同步机制，两个CPU同时操作空闲列表（一个链表）会导致问题。

#### 无锁的链表操作

考虑文中给出的简单链表push()实现：

```
struct element {
    int data;
    struct element *next;
};
struct element *list = 0;

void push(int data) {
    struct element *l;
    l = malloc(sizeof *l);  // 分配新元素
    l->data = data;         // 设置数据
    l->next = list;         // 新元素的next指向当前list
    list = l;               // 更新list指向新元素
}
```

- **正常情况**（单线程）：
    - 按顺序执行，list正确指向新元素，新元素的next指向旧的list，链表保持一致。
- **并发情况**（多线程无锁）：
    - 假设两个CPU（CPU1和CPU2）同时调用push()：
        1. CPU1：l1 = malloc(); l1->data = 1; l1->next = list;（此时list = 0）。  
        2. CPU2：l2 = malloc(); l2->data = 2; l2->next = list;（此时list = 0）。  
        3. CPU1：list = l1;（list指向l1，l1->next = 0）。  
        4. CPU2：list = l2;（list指向l2，l2->next = 0）。  
    - **结果**：
        - list最终指向l2，而l1丢失（未链接到链表）。
        - 这就是“丢失更新”的***竞态条件***。

**图示**
![[Pasted image 20250405212553.png]]

```
初始：list = 0
CPU1：l1->next = 0, list = l1
CPU2：l2->next = 0, list = l2
最终：list -> l2 -> 0（l1丢失）
```

#### 为什么会出错？

- **共享数据**：list是全局变量，两个CPU同时读写。
- **时序依赖**：l->next = list和list = l的执行顺序未定义，导致覆盖或不一致。
- **难以调试**：竞态条件依赖时序，添加调试语句可能改变时序，使问题消失。

---

### 2. 为什么需要锁？

锁（lock）通过互斥（mutual exclusion）解决竞态条件，确保一次只有一个进程能访问共享数据的临界区。

#### 加锁版本

```
struct element *list = 0;
struct lock listlock;

void push(int data) {
    struct element *l;
    l = malloc(sizeof *l);
    l->data = data;
    acquire(&listlock);    // 获取锁
    l->next = list;        // 临界区开始
    list = l;              // 更新list
    release(&listlock);    // 释放锁
}
```

- **临界区**：acquire和release之间的代码，一次只能由一个CPU执行。
- **结果**：
    - CPU1执行完push(1)后，list -> l1 -> 0。
    - CPU2等待锁释放后执行push(2)，list -> l2 -> l1 -> 0。
    - 链表保持正确，没有元素丢失。

#### 锁如何工作？

- **互斥性**：acquire()确保只有一个CPU进入临界区，其他CPU等待。
- **串行化**：并发操作被强制按顺序执行，消除了时序依赖。
- **保护不变量**：
    - 不变量：list指向链表头，每个next指向下一个元素。
    - 无锁时，l->next = list后list未更新，可能违反不变量。
    - 加锁后，临界区内的操作**原子**完成，不变量始终成立。

---

### 3. 类比与举例

#### 类比：银行取款

- **场景**：两个人（A和B）同时从同一个账户（余额100元）取50元。
- **无锁**：
    
    1. A读取余额（100），计算100-50=50。
    2. B读取余额（100），计算100-50=50。
    3. A写入余额50。
    4. B写入余额50。
    
    - 结果：余额变成50（应为0），丢失了一次扣款。
- **加锁**：
    - A获取锁，读取100，写入50，释放锁。
    - B等待，获取锁，读取50，写入0，释放锁。
    - 结果：余额正确为0。

#### 例子：xv6的kfree()

- **场景**：两个CPU调用kfree()释放页面到空闲列表。
- **无锁**：
    - freelist是一个链表。
    - CPU1：page1->next = freelist; freelist = page1;
    - CPU2：page2->next = freelist; freelist = page2;
    - 可能结果：freelist -> page2 -> 旧freelist，page1丢失。
- **加锁**：
    - 加锁后，CPU1完成freelist -> page1 -> 旧freelist，CPU2再追加page2，结果正确。

---

### 4. 锁的性能影响

#### 优点

- **正确性**：避免竞态条件，保证数据一致性。
- **简单性**：通过串行化简化并发逻辑。

#### 缺点

- **性能限制**：
    - 锁将并发操作串行化，多个CPU无法并行执行kfree()。
    - **锁争用（contention）**：多个进程竞争同一锁时，等待时间增加。
- **例子**：
    - 无锁时，两个CPU并行kfree()，性能翻倍。
    - 加锁后，第二个CPU必须等待，性能下降。

#### 优化策略

- **细粒度锁**：
    - 为每个CPU维护独立空闲列表，仅在必要时（如列表为空）加锁访问共享资源。
    - 例如：freelist_cpu1 和 freelist_cpu2，减少争用。
- **锁位置**：
    - 文中提到将acquire移到malloc()前是正确的，但会串行化malloc()，降低性能。
    - 最佳实践：锁只保护必要的最小临界区。

---

### 5. 使用锁的指导方针

#### 锁放置原则

1. **最小化临界区**：
    - 只锁住操作共享数据的代码（如l->next = list; list = l;）。
    - 避免锁住无关操作（如malloc()）。
2. **保护不变量**：
    - 确定数据结构的不变量（如链表的链接关系），锁住可能违反不变量的代码。
3. **避免死锁**：
    - 多个锁时，按固定顺序获取（如先lock1后lock2）。
4. **性能权衡**：
    - 粗粒度锁（大临界区）简单但性能差。
    - 细粒度锁（小临界区）复杂但并发性高。

#### 示例改进

```
struct element *list = 0;
struct lock listlock;

void push(int data) {
    struct element *l = malloc(sizeof *l);  // 无需锁
    l->data = data;                         // 无需锁
    acquire(&listlock);                     // 锁住共享数据
    l->next = list;
    list = l;
    release(&listlock);
}
```

- **优化**：malloc()和l->data**不涉及共享数据，放在锁外提高并发性。**
