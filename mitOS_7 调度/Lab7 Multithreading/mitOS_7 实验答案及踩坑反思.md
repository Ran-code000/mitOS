
# Lab7: Multithreading

---
# Uthread: switching between threads (moderate)

(1) 定义存储上下文的结构体`context`
```

// Saved registers for user context switches.
struct context {
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};
```
(2) 修改`thread`结构体，添加`context`字段
```
struct thread {
  char       stack[STACK_SIZE]; /* the thread's stack */
  int        state;             /* FREE, RUNNING, RUNNABLE */
  struct context context;
};
```
(3) 模仿 _kernel/swtch.S，_在_user/uthread_switch.S_中写入如下代码
```
        .text

        /*
         * save the old thread's registers,
         * restore the new thread's registers.
         */

        .globl thread_switch
thread_switch:
        /* YOUR CODE HERE */
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
        ret    /* return to ra */
```
(4) 修改`thread_scheduler`，添加线程切换语句
```
  if (current_thread != next_thread) {         /* switch threads?  */
    next_thread->state = RUNNING;
    t = current_thread;
    current_thread = next_thread;
    /* YOUR CODE HERE
     * Invoke thread_switch to switch from t to next_thread:
     * thread_switch(??, ??);
     */
     thread_switch((uint64)&t->context, (uint64)&current_thread->context); //添加
  } else
    next_thread = 0;
```
(5) 在`thread_create`中对`thread`结构体做一些初始化设定，主要是`ra`返回地址和`sp`栈指针，其他的都不重要
```
void
thread_create(void (*func)())
{
  struct thread *t;

  for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
    if (t->state == FREE) break;
  }
  t->state = RUNNABLE;
  // YOUR CODE HERE
  t->context.ra = (uint64)func;
  t->context.sp = (uint64)t->stack + STACK_SIZE;
}
```
(6) 在 Makefile 中添加
```
UPROGS=\
  $U/_cat\
  $U/_echo\
  $U/_forktest\
  $U/_ls\
  $U/_uthread\ //添加
```
---
# Using threads (moderate)

JOB1：
为什么为造成数据丢失：

> 假设现在有两个线程T1和T2，两个线程都走到put函数，且假设两个线程中key%NBUCKET相等，即要插入同一个散列桶中。两个线程同时调用insert(key, value, &table[i], table[i])，insert是通过头插法实现的。如果先insert的线程还未返回另一个线程就开始insert，那么前面的数据会被覆盖

因此只需要对插入和读取操作上锁即可
（1）declare a lock
```
int keys[NKEYS];
int nthread = 1;
volatile int done;
pthread_mutex_t lock; //添加
```
（2）main 函数中初始化（全局）锁
```
int
main(int argc, char *argv[])
{
  pthread_t *tha;
  void *value;
  long i;
  double t1, t0;

  pthread_mutex_init(&lock, NULL); //添加
```
（3）修改 put 和 get 函数
```
static
void put(int key, int value)
{
  int i = key % NBUCKET;
  pthread_mutex_lock(&lock); //添加
  insert(key, value, &table[i], table[i]);
  pthread_mutex_unlock(&lock); //添加
}

static struct entry*
get(int key)
{
  struct entry *e = 0;
  pthread_mutex_lock(&lock); //添加
  for (e = table[key % NBUCKET]; e != 0; e = e->next) {
    if (e->key == key) break;
  }
  pthread_mutex_unlock(&lock); //添加
  return e;
}
```

JOB2：
- 如果使用单一全局锁（如 pthread_mutex_t lock），所有线程在访问任何桶时都会串行化，无法并行。

目标
- 为每个散列桶分配一个独立的锁。
- 线程操作不同桶时可以并行，只有操作同一桶时才需要等待，从而提高 put() 的并行性。

（1）为每个散列桶定义一个锁，将五个锁放在一个数组中，并进行初始化
```
struct entry *table[NBUCKET];
int keys[NKEYS];
int nthread = 1;
volatile int done;
pthread_mutex_t locks[NBUCKET]; //添加
```
（2）main 函数中初始化每个散列桶的锁
```
int
main(int argc, char *argv[])
{
  pthread_t *tha;
  void *value;
  long i;
  double t1, t0;

  //添加
  for (i = 0; i < NBUCKET; i++) {
    pthread_mutex_init(&locks[i], NULL);
  }
```
（3）修改 put 和 get 函数
```
static
void put(int key, int value)
{
  int i = key % NBUCKET;
  pthread_mutex_lock(&locks[i]); //添加
  insert(key, value, &table[i], table[i]);
  pthread_mutex_unlock(&locks[i]); //添加
}

static struct entry*
get(int key)
{
  struct entry *e = 0;
  pthread_mutex_lock(&locks[key % NBUCKET]); //添加
  for (e = table[key % NBUCKET]; e != 0; e = e->next) {
    if (e->key == key) break;
  }
  pthread_mutex_unlock(&locks[key % NBUCKET]); //添加
  return e; 
}
```

---
# Barrier(moderate)

 实现思路

- **加锁**：
    - 使用pthread_mutex_lock(&bstate.barrier_mutex) 保护共享变量。  
- **计数**：
    - 每次线程到达，bstate.nthread++。
- **等待**：
    - 如果 bstate.nthread < nthread，用 pthread_cond_wait 等待。
- **唤醒**：
    - 当 bstate.nthread == nthread：  
        - 递增 bstate.round。
        - 重置 bstate.nthread = 0。
        - 用 pthread_cond_broadcast 唤醒所有等待线程。
- **解锁**：
    - 释放互斥锁。

```
static void
barrier()
{
  pthread_mutex_lock(&bstate.barrier_mutex);
  bstate.nthread++;
  if(bstate.nthread < nthread){
    pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
  }else{
    bstate.nthread = 0;
    bstate.round++;
    pthread_cond_broadcast(&bstate.barrier_cond);
  }
  pthread_mutex_unlock(&bstate.barrier_mutex);

}
```