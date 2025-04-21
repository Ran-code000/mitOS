### 总结重点

1. **分配器位置**:
    - 在 kalloc.c 中，用链表管理空闲页面。
2. **数据结构**:
    - struct run 存储在页面本身，链表由 kmem.freelist 维护。
如lab2中- 在_kernel/kalloc.c_中添加一个函数用于获取空闲内存量用到 kmem.freelist
```
void
freebytes(uint64 *dst)
{
  *dst = 0;
  struct run *p = kmem.freelist; // 用于遍历

  acquire(&kmem.lock);
  while (p) {
    *dst += PGSIZE;
    p = p->next;
  }
  release(&kmem.lock);
}
```
3. **初始化**:
    - kinit 和 freerange 将内核末尾到 PHYSTOP 的页面加入链表。
4. **操作**:
    - kfree 填充 1 并头插，kalloc 移除并返回首个页面。
5. **地址使用**:
    - 整数运算和指针操作并存，需类型转换。
---

#### 1. **分配器的数据结构**

- **概念**:
    - 分配器维护一个空闲页面链表，每个节点是 struct run。
    - struct run 定义：
```
struct run {
  struct run *next;
};    
```

- 空闲页面本身存储 struct run，因为页面空闲时没有其他数据
    
- **类比**:
    - 像一个“空闲货架清单”，每个货架（页面）上贴着一张纸条（struct run），写着下一个空货架的地址。
- **示例**:
    - 空闲页面 0x80101000：
        - 前 8 字节存储 struct run，其中 next = 0x80102000。
        - 链表：0x80101000 -> 0x80102000 -> 0x80103000。

#### 2. **自旋锁保护**

- **概念**:
    - 空闲链表由自旋锁保护，定义在 struct kmem 中：
        
```
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;
```

- 自旋锁：调用者循环等待锁释放，不睡眠。

- **类比**:
    - 像仓库管理员（锁）看守清单，只有拿到钥匙（锁）才能动货架（链表），没拿到钥匙就站着等（自旋）。
- **示例**:
    - 线程 A 获取锁修改链表，线程 B 调用 acquire 循环等待，直到 A 释放。

#### 3. **初始化 (kinit 和 freerange)**
```
void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}
```
- **过程**:
    - main 调用 kinit 初始化分配器。
    - 假设机器有 128 MB RAM，从内核末尾（例如 0x80100000）到 PHYSTOP=0x88000000。
    - freerange 将每页加入空闲链表，通过 kfree 添加。
    - 使用 PGROUNDUP 确保地址对齐 4096 字节。
	- **关键变量**：
		- end：全局变量，表示***内核代码和数据的末尾地址（由链接器定义）***。
		- PHYSTOP：常量，表示物理内存的上限（例如 0x88000000，128 MB）。
- **类比**:
    - 像仓库开业，管理员把所有空货架（页面）登记到清单（链表），确保货架编号整齐（对齐）。
- **示例**:
    - 内核末尾 0x80100000，PHYSTOP = 0x88000000。  
    - freerange(0x80101000, 0x88000000)：  
        - 调用 kfree(0x80101000)，kfree(0x80102000)，...  
        - 链表：0x80101000 -> 0x80102000 -> ...

#### 4. **释放内存 (kfree)**
```
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}
```
- **过程**:
    - kfree 将页面填充为 1（防止悬空引用），然后头插到链表。
    - 类型转换：物理地址转为 struct run *。
- **类比**:
    - 像归还货架，先清空货物填满标记（1），然后把货架编号写到清单最前面。
- **示例**:
    - kfree(0x80101000)：  
        - 填充 0x80101000 - 0x80101FFF 为 1。
        - 原链表：0x80102000 -> 0x80103000。
        - r = (struct run *)0x80101000，r->next = 0x80102000。  
        - 新链表：0x80101000 -> 0x80102000 -> 0x80103000。

#### 5. **分配内存 (kalloc)**
```
// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
```
- **过程**:
    - kalloc 从链表头部取出一个页面，返回其地址。
- **类比**:
    - 像从清单上拿走第一个空货架，交给工人使用。
- **示例**:
    - kalloc()： 
        - 原链表：0x80101000 -> 0x80102000 -> 0x80103000。
        - 返回 0x80101000，新链表：0x80102000 -> 0x80103000。

#### 6. **地址的双重用途**

- **概念**:
    - 地址既作为整数运算（如遍历页面），又作为指针操作（如设置 next）。
    - 类型转换频繁（如 char* 到 struct run*）。
- **类比**:
    - 像货架编号既是数字（计算顺序），又是地址（找到货架）。
- **示例**:
    - freerange：用整数 pa += PGSIZE 遍历。
    - kfree：用 (struct run *)pa 设置 next。  

---

### 综合示例

假设初始化并操作内存：

1. **初始化**:
    - kinit() 设置 0x80101000 - 0x88000000。  
    - freerange 调用 kfree，链表：0x80101000 -> 0x80102000 -> 0x80103000。
2. **分配**:
    - kalloc() 返回 0x80101000，链表：0x80102000 -> 0x80103000。  
    - 用于用户内存。
3. **释放**:
    - kfree(0x80101000)：  
        - 填充 1。
        - 链表：0x80101000 -> 0x80102000 -> 0x80103000。
4. **再分配**:
    - kalloc() 返回 0x80101000，链表：0x80102000 -> 0x80103000。  

---

### 类比补充

- **仓库管理**:
    - 仓库（物理内存）有货架（页面），清单（链表）记录空货架。
    - 开业时（kinit）登记所有货架。
    - 借货架（kalloc）从清单拿走，归还（kfree）填标记并加回。

