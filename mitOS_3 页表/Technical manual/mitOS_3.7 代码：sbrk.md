
### 总结重点

1. **sbrk 实现**:
    - growproc 根据 n 调用 uvmalloc（增长）或 uvmdealloc（缩减）。
2. **内存增长**:
    - uvmalloc 用 kalloc 分配页面，mappages 更新页表。
3. **内存缩减**:
    - uvmdealloc 用 uvmunmap，walk 查找 PTE，kfree 释放页面。  
4. **页表作用**:
    - 不仅是地址映射，也是已分配内存的记录，释放时需检查。
---
#### 1. **sbrk 系统调用与 growproc**

```
uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}
```

- **概念**:
    - sbrk 允许进程增加或减少内存，返回新的内存边界。
    - growproc（kernel/proc.c:239）根据参数 n（字节数）决定增长还是缩减：
        - n > 0: 调用 uvmalloc 分配内存。
        - n < 0: 调用 uvmdealloc 释放内存。
    growproc：
```
// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}
```
```

```
uvmalloc，uvmdealloc：

```
// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}
```
kalloc，kfree

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
- **类比**:
    - 像住户（进程）向物业（内核）申请扩建或缩小房子，物业根据需求加房间（uvmalloc）或拆房间（uvmdealloc）。
- **示例**:
    - 进程调用 sbrk(4096)（增加 4 KB）：
        - growproc(4096) 调用 uvmalloc。  
    - 进程调用 sbrk(-4096)（减少 4 KB）：
        - growproc(-4096) 调用 uvmdealloc。  

---

#### 2. **内存增长 (uvmalloc)**

- **过程**:
    - uvmalloc（kernel/vm.c:229）：  
        - 用 kalloc 分配物理页面。
        - 用 mappages 将 PTE 添加到用户页表，设置权限（PTE_V | PTE_R | PTE_W | PTE_U）。
- **类比**:
    - 像物业从仓库（物理内存）拿砖块（页面），在住户地图（页表）上标出新房间的位置。
- **示例**:
    - uvmalloc(0x1000, 0x2000, ...)（从 0x1000 增长到 0x2000）：
        - kalloc() 返回 0x80101000。  
        - mappages 设置：VA 0x1000 -> PPN=0x80101 | PTE_V | PTE_R | PTE_W | PTE_U。  

---

#### 3. **内存缩减 (uvmdealloc 和 uvmunmap)**

- **过程**:
    - uvmdealloc 调用 uvmunmap（kernel/vm.c:174）：  
        - 用 walk 查找 PTE。
        - 检查 PTE 是否有效（PTE_V），若有效，用 kfree 释放物理页面，清空 PTE。
- **类比**:
    - 像物业检查住户地图（页表），找到要拆的房间（PTE），把砖块（页面）放回仓库（kfree），擦掉地图记录。
- **示例**:
    - uvmdealloc(0x2000, 0x1000)（从 0x2000 缩减到 0x1000）：
        - uvmunmap(0x1000, 4096)：  
            - walk 找到 VA 0x1000 的 PTE（例如 0x80101000 | PTE_V）。  
            - kfree(0x80101000)，PTE 设置为 0。  

---

#### 4. **页表的双重作用**

- **概念**:
    - 页表不仅告诉硬件如何映射虚拟地址到物理地址，还是进程已分配物理内存的唯一记录。
    - 释放内存时必须检查页表，确认哪些页面已分配。
- **类比**:
    - 像住户的地图不仅是导航工具，还是物业记录哪些房间归住户的账本，拆房前要查账本确认。
- **示例**:
    - 进程页表：
        - VA 0x1000 -> PPN=0x80101 | PTE_V（已分配）。  
        - VA 0x2000 -> 0（未分配）。
    - uvmunmap(0x1000, 8192)：  
        - 检查 0x1000 的 PTE，释放 0x80101000。
        - 检查 0x2000 的 PTE，无效，不释放。

---

### 综合示例

假设进程初始内存从 0x0 到 0x1000，栈在 0x7FFFFFF000：

1. **增加内存**:
    - 调用 sbrk(4096)：
        - growproc(4096) -> uvmalloc(0x1000, 0x2000)。  
        - kalloc() 返回 0x80102000。  
        - mappages 设置：VA 0x1000 -> PPN=0x80102 | PTE_V | PTE_R | PTE_W | PTE_U。  
        - 新内存边界：0x2000。
2. **减少内存**:
    - 调用 sbrk(-4096)：
        - growproc(-4096) -> uvmdealloc(0x2000, 0x1000)。  
        - uvmunmap(0x1000, 4096)：  
            - walk 找到 PTE，释放 0x80102000，PTE = 0。  
        - 新内存边界：0x1000。
3. **页表检查**:
    - 页表记录：
        - VA 0x0 -> PPN=0x80101 | PTE_V（保留）。  
        - VA 0x1000 -> 0（已释放）。

---

### 类比补充

- **小区物业管理**:
    - 住户（进程）找物业（内核）要房间（内存）。
    - 增加房间：物业拿砖块（kalloc），画在地图上（mappages）。
    - 减少房间：物业查地图（walk），拆砖块放回仓库（kfree）。
    - 地图（页表）是房间分配的唯一依据。