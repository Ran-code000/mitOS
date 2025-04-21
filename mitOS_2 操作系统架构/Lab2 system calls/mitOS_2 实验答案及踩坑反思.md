
# Lab2: system calls
---

# System call tracing（moderate）


总述：在用户空间输入命令 `trace 32 grep hello README`，操作系统找到并执行 user/trace.c 生成的可执行文件。

- **补充**：
    - “user/ 目录下包含用于生成用户空间可执行程序的源代码和相关支持文件。”
- **解释**：
    - user/ 目录是 XV6 项目中存放用户态程序相关代码的地方。
    - 这些代码经过编译（由 Makefile 处理），生成可以在用户空间运行的可执行程序（例如 trace、grep），最终被放入文件系统（如 fs.img）供 XV6 shell 执行。
    - 除了源代码，还有辅助文件（如 usys.pl 生成系统调用存根），它们本身不直接执行，但支持用户态程序与内核交互。

**疑问：**
**trace 函数**实现在哪里？

- **用户态**：没有实现，只有存根（user/usys.S）。
- **内核态**：实现为 sys_trace（kernel/sysproc.c）。
- **为什么 grep.c 有 grep 函数，而 trace.c 没有 trace 函数**：
    - grep.c：完整的用户程序，main 调用 grep 函数实现逻辑。
    - trace.c：辅助工具，main 调用系统调用 trace（内核实现），然后执行其他程序。

*执行命令 `trace 32 grep hello README`  的完整流程：*

## （1）用户态：trace.c 执行

trace.c文件如下

```
#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int i;
  char *nargv[MAXARG];

  if(argc < 3 || (argv[1][0] < '0' || argv[1][0] > '9')){
    fprintf(2, "Usage: %s mask command\n", argv[0]);
    exit(1);
  }

  if (trace(atoi(argv[1])) < 0) {
    fprintf(2, "%s: trace failed\n", argv[0]);
    exit(1);
  }

  for(i = 2; i < argc && i < MAXARG; i++){
    nargv[i-2] = argv[i];
  }
  exec(nargv[0], nargv);
  exit(0);
}
```

- atoi(argv[1]) = 32， trace(atoi(argv[1]))
- 
## （2）调用 trace(32)：用户态接口  

首先：
- user/user.h：添加 int trace(int mask); 
		声明 trace 函数原型，让用户程序知道如何调用。
- user/usys.pl：添加 entry("trace")，生成 user/usys.S。
		一个 Perl 脚本，自动生成汇编存根，自动化创建系统调用入口。
		**作用**：桥接用户态和内核态
- kernel/syscall.h：添加 `#define SYS_trace 22`
		定义系统调用号 SYS_trace（一个唯一的整数标识某个系统调用），供用户态和内核态共享。

**存根**（user/usys.S）：
```
.global trace
trace:
    li a7, SYS_trace  // a7 = 22
    ecall
    ret
```

- **为什么找存根**：存根是用户态调用内核态的桥梁，封装了 **ecall 指令**，直接写 C 代码无法陷入内核。
- **怎么找到**：编译时，trace 被链接到 usys.S 中的 .global trace 符号。当 C 代码调用 trace(32)，链接器将调用指向这个汇编代码。
- **寄存器**：
    - a7：保存系统调用号（SYS_trace = 22），告诉内核执行哪个系统调用。
    - a0：保存第一个参数（32），由 C 调用约定自动传递。
    - a1：未使用（trace 只有一个参数）。

## (3) ecall 陷入内核态

- **疑问**：为什么执行 kernel/syscall.c？
    - **原因**：ecall 触发 RISC-V 的异常，硬件将控制权交给内核的陷阱处理程序（kernel/trap.c 的 **usertrap**），它最终**调用** kernel/syscall.c 的 syscall() 函数。这是 XV6 的固定流程，不是随意规定的。

## （4）内核态：syscall() 执行

**代码**：
```
...


extern uint64 sys_chdir(void);
extern uint64 sys_close(void);
extern uint64 sys_dup(void);
extern uint64 sys_exec(void);
extern uint64 sys_exit(void);
extern uint64 sys_fork(void);
extern uint64 sys_fstat(void);
extern uint64 sys_getpid(void);
extern uint64 sys_kill(void);
extern uint64 sys_link(void);
extern uint64 sys_mkdir(void);
extern uint64 sys_mknod(void);
extern uint64 sys_open(void);
extern uint64 sys_pipe(void);
extern uint64 sys_read(void);
extern uint64 sys_sbrk(void);
extern uint64 sys_sleep(void);
extern uint64 sys_unlink(void);
extern uint64 sys_wait(void);
extern uint64 sys_write(void);
extern uint64 sys_uptime(void);
extern uint64 sys_trace(void); //

static uint64 (*syscalls[])(void) = { 
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
[SYS_pipe]    sys_pipe,
[SYS_read]    sys_read,
[SYS_kill]    sys_kill,
[SYS_exec]    sys_exec,
[SYS_fstat]   sys_fstat,
[SYS_chdir]   sys_chdir,
[SYS_dup]     sys_dup,
[SYS_getpid]  sys_getpid,
[SYS_sbrk]    sys_sbrk,
[SYS_sleep]   sys_sleep,
[SYS_uptime]  sys_uptime,
[SYS_open]    sys_open,
[SYS_write]   sys_write,
[SYS_mknod]   sys_mknod,
[SYS_unlink]  sys_unlink,
[SYS_link]    sys_link,
[SYS_mkdir]   sys_mkdir,
[SYS_close]   sys_close,
[SYS_trace]   sys_trace, //
};

static char *syscalls_name[] = {
[SYS_fork]    "fork",
[SYS_exit]    "exit",
[SYS_wait]    "wait",
[SYS_pipe]    "pipe",
[SYS_read]    "read",
[SYS_kill]    "kill",
[SYS_exec]    "exec",
[SYS_fstat]   "fstat",
[SYS_chdir]   "chdir",
[SYS_dup]     "dup",
[SYS_getpid]  "getpid",
[SYS_sbrk]    "sbrk",
[SYS_sleep]   "sleep",
[SYS_uptime]  "uptime",
[SYS_open]    "open",
[SYS_write]   "write",
[SYS_mknod]   "mknod",
[SYS_unlink]  "unlink",
[SYS_link]    "link",
[SYS_mkdir]   "mkdir",
[SYS_close]   "close",
[SYS_trace]   "trace", //
};

void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    p->trapframe->a0 = syscalls[num]();
     // 系统调用是否匹配
  if ((1 << num) & p->trace_mask)
      printf("%d: syscall %s -> %d\n", p->pid, syscalls_name[num], p->trapframe->a0);

  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
```
- **疑问**：`static uint64 (*syscalls[])(void)` 是什么？
    - **解释**：这是一个函数指针数组：
        - uint64 (*)(void)：函数指针类型，返回 uint64，无参数。
        - static：仅在此文件可见。
        - `[SYS_trace] sys_trace`：索引为 SYS_trace (本例为23)的元素指向 sys_trace 函数。
    - **作用**：根据系统调用号（num）快速查找并调用对应的内核函数。
- **流程**：
    - `num = p->trapframe->a7`：从陷阱帧(trapframe)取 a7 = 22。
	    （a7：保存系统调用号（SYS_trace = 22），告诉内核执行哪个系统调用。）
    - `syscalls[22]()`：调用 sys_trace。

## (5) 执行 sys_trace（kernel/sysproc.c）

代码：
```
uint64 sys_trace(void) {
    int mask;
    if (argint(0, &mask) < 0)  // 从 a0 获取掩码
        return -1;
    myproc()->trace_mask = mask;  // 将参数保存到 proc 结构体
    return 0;
}
```

- **疑问**：struct proc 是什么？与 PCB 关系？
	- **定义**：struct proc 是 XV6 对进程的描述，包含状态、栈、页面表等。
	- **每个进程都有吗**：是的，每个进程对应一个 struct proc，存储在内核的 proc 数组中。
	- **与 PCB**：PCB（Process Control Block）是操作系统的通用概念，struct proc 是 XV6 的具体实现，二者等价。

- **疑问**：为什么找到 kernel/sysproc.c？
    - **原因**：sys_trace 的实现定义在 kernel/sysproc.c 中（由你添加），syscall.c 的 syscalls 表指向它。XV6 将大多数系统调用实现放在 sysproc.c 中，这是代码组织惯例。
    
- **疑问**：为什么通过 a0 获取参数？
    - **原因**：RISC-V 的 C 调用约定规定，第一个参数传到 a0。用户态调用 trace(32) 时，32 被放入 a0，内核用 argint(0, &mask) 从 p->trapframe->a0 提取。
    - **argint(0, &mask)**：从陷阱帧的第 0 个参数（a0）取值。
    - **uservec**（kernel/trap.c）：  
        - 保存用户寄存器到陷阱帧（p->trapframe），跳转到 usertrap。
    - **usertrap**： 
        - 设置内核态环境，调用 syscall 处理系统调用。
    - 陷阱帧保存：
        - p->trapframe->a0 = 32
        - p->trapframe->a1 = 无
        - p->trapframe->a7 = 5

## (6) 保存掩码与返回

- **保存**：myproc()->trace_mask = 32。
- **返回**：sys_trace 返回 0，存到 p->trapframe->a0。
- **疑问**：返回的是什么？trapframe 是什么？
    - **返回**：0 表示成功，负数表示失败。
    - **trapframe**：一个结构体（kernel/proc.h），保存用户态寄存器状态：
```
struct trapframe {
    uint64 ra;  // 返回地址
    uint64 a0;  // 参数/返回值
    uint64 a7;  // 系统调用号
    // 其他寄存器...
};
```
- **作用**：保存用户态上下文，a0 用于传递返回值给用户程序。
- **为什么保存到 a0**：RISC-V 约定返回值放在 a0，回到用户态时，程序从 a0 获取结果。

## (7) 回到用户态：继续 trace.c

- **syscall 返回**：返回到内核的陷阱处理程序 usertrap，再回到 user/usys.S 的 ret，然后回到 trace.c 的 trace(32) 调用点。

**执行**：
```
for (i = 2; i < argc; i++) {
    nargv[i-2] = argv[i];  // nargv = ["grep", "hello", "README"]
}
exec(nargv[0], nargv);  // 执行 grep
```

- trace(32) 返回后立即执行exec。
- 替换当前进程映像为 grep，保留 trace_mask = 32，开始执行 grep。

## (8) grep 执行与跟踪

- **grep 的系统调用**：如 open、read、close。
- **跟踪**：
    - 每次系统调用，syscall() 检查 p->trace_mask & (1 << num)：
        - 如：SYS_read = 5，1 << 5 = 32，匹配 trace_mask = 32
    打印：
```
3: syscall read -> 1023
```

**疑问**：父进程到子进程如何追踪？
- **实现**：在 kernel/proc.c 的 fork() 中：添加将父进程的mask复制到子进程的mask中的代码
```
np->trace_mask = p->trace_mask;  // 子进程复制父进程掩码
```

**例子**：trace 2147483647 grep hello README：

- trace_mask = 2147483647（全 1），跟踪所有系统调用。
- exec 后，grep 的子进程（如果有）继承掩码，继续跟踪。

## (9) 退出

- **exec 失败或完成**：若 exec 失败，exit(0) 终止进程；若成功，grep 运行结束。
- **疑问**：exit(0) 到哪里？
    - **答案**：调用 SYS_exit，陷入内核，sys_exit 清理进程，通知父进程（shell）。

---
# Sysinfo（moderate）

实现步骤总结：
1. 修改 user/user.h 添加用户空间接口：声明`sysinfo()`的原型
2. 更新 Makefile 添加测试程序：`$U/_sysinfotest`
3. 修改 kernel/syscall.h 添加系统调用号：`#define SYS_sysinfo 23`
4. 修改 kernel/syscall.c 注册系统调用
		`extern uint64 sys_sysinfo(void);`
		`[SYS_sysinfo] sys_sysinfo,`
		`[SYS_sysinfo] "sysinfo",`
5. 在 kernel/kalloc.c 实现 freebytes()
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
内存是使用链表进行管理的，因此遍历`kmem`中的空闲链表就能够获取所有的空闲内存

6. 在 kernel/proc.c 实现 procnum()
```
void
procnum(uint64 *dst)
{
  *dst = 0;
  struct proc *p;
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state != UNUSED)
      (*dst)++;
  }
}
```
遍历`proc`数组，统计处于活动状态的进程即可，循环的写法参考`procdump` 函数

7. 在 defs.h 中声明 freebytes() 和 procnum()
		原因：kernel/defs.h: 全局函数声明的集中位置，被多个源文件包含。
		
8. 在 kernel/sysproc.c 实现 sys_sysinfo
```
uint64
sys_sysinfo(void)
{
  struct sysinfo info;
  freebytes(&info.freemem);
  procnum(&info.nproc);

  // 获取虚拟地址
  uint64 dstaddr;
  argaddr(0, &dstaddr);

  // 从内核空间拷贝数据到用户空间
  if (copyout(myproc()->pagetable, dstaddr, (char *)&info, sizeof info) < 0)
    return -1;

  return 0;
}
```
- 实现`sys_sysinfo`，将数据写入结构体并传递到用户空间

6. 创建 kernel/sysinfo.h 定义结构
```
#ifndef SYSINFO_H
#define SYSINFO_H

struct sysinfo {
    uint64 freemem;
    uint64 nproc;
};


#endif
```


