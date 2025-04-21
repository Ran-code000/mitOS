
## **fork()**


每次 fork() 只创建一个子进程。fork() 不是直接返回两个 PID，而是返回一个值，这个值在父进程和子进程中被解释为不同的含义


- **fork() 的作用**：
    - 创建一个新的子进程，复制父进程的内存和状态。
    - 返回一个整数值给调用者（父进程和子进程都会收到返回值）。
- **返回值规则**：
    - **在父进程中**：fork() 返回子进程的 PID（一个正整数，例如 101）。
    - **在子进程中**：fork() 返回 0。
    - **如果失败**：返回 -1（例如，没有足够的内存创建进程）。


```
// fork()在父进程中返回子进程的PID
// 在子进程中返回0
int pid = fork();
if(pid > 0) {
    printf("parent: child=%d\n", pid);
    pid = wait((int *) 0);
    printf("child %d is done\n", pid);
} else if(pid == 0) {
    printf("child: exiting\n");
    exit(0);
} else {
    printf("fork error\n");
}

```

### 用类比理解

想象你在餐厅点了一份披萨（调用 fork()）：

- **餐厅（内核）**：
    - 做了一份新披萨（子进程），编号是 101。
    - 给你（父进程）一张纸条，写着“你的新披萨是 101号”。
    - 给新披萨（子进程）一张纸条，写着“你是 0号（新来的）”。
- **你（父进程）**：
    - 看到纸条，知道新披萨是 101。
- **新披萨（子进程）**：
    - 看到纸条，知道自己是“0”（表示“我是被创建的”），但实际编号是 101

---

## **exec()**


#### fork 的区别：

- fork 创建一个新进程，**复制**父进程的内存。
- exec 不创建新进程，而是改造当前进程，用新程序**替换**原有内容

```
#include "types.h"
#include "stat.h"
#include "user.h"

int main() {
    char *argv[3];
    argv[0] = "echo";      // 程序名（惯例）
    argv[1] = "hello";     // 参数
    argv[2] = 0;           // 数组结束标志
    exec("/bin/echo", argv);
    printf(1, "exec error\n"); // 只有 exec 失败时才会执行
    exit(0);
}
```

### exec 的执行过程

#### 假设场景：

- 当前进程（PID = 100）运行上述代码。
- 文件 /bin/echo 是一个 ELF 格式的可执行文件，功能是打印传入的参数。

#### 步骤分解：

1. **调用 exec**：
    - 当前进程执行 exec("/bin/echo", argv)。
    - 系统查找文件 /bin/echo，确认它存在且是 ELF 格式。
2. **加载 ELF 文件**：
    - **ELF Header**：读取文件的头部，获取关键信息：
        - 代码段（.text）：存放 echo 的指令。
        - 数据段（.data）：存放初始化的全局变量。
        - 入口点（entry point）：例如地址 0x1000，程序开始执行的位置。
    - 系统分配新内存，将这些段加载到进程的地址空间。
3. **替换内存映像**：
    - 当前进程的原有内存（包括 main 函数的代码、栈等）被释放。
    - 新的内存映像（/bin/echo 的内容）替换进来。
    - 参数 argv 被传递给新程序。
4. **开始执行新程序**：
    - 进程跳转到 /bin/echo 的入口点（例如 0x1000）。
    - /bin/echo 读取 argv，打印 "hello"。  
    - 执行完成后，/bin/echo 调用 exit，进程结束。
5. **通常不返回**：
    - 如果 exec 成功，printf("exec error\n") 不会执行，因为原程序已被替换。
    - 如果失败（例如文件不存在），返回 -1，执行后续代码。
#### 简单理解：

- ELF 文件像一本书，包含：
    - **目录（Header）**：告诉系统哪里是代码、哪里是数据、从哪开始。
    - **章节（Sections/Segments）**：实际的指令和数据。
- Xv6 通过 ELF 解析，确保新程序正确加载。

- **exec**：  **exec** 中：
    - 用新程序（从文件加载）替换当前进程的内存。
    - 成功后，从新程序的入口点开始执行，不返回。
    - 
- **ELF 格式**：
    - 指定代码、数据和入口点，确保正确加载。
- **例子**：
    - exec("/bin/echo", ["echo", "hello", 0]) 替换进程，运行 echo，输出 "hello

---

## **Shell 执行 "echo hello" 的过程**

```
#include "types.h"
#include "user.h"
#include "fcntl.h"

void runcmd(char *cmd);

int main() {
    static char buf[100];
    while (1) {
        printf(1, "$ ");           // 提示符
        getcmd(buf, sizeof(buf));  // 读取输入，例如 "echo hello"
        if (fork() == 0) {         // 子进程
            runcmd(buf);           // 执行命令
        } else {                   // 父进程
            wait((int *) 0);       // 等待子进程完成
        }
    }
    exit(0);
}

void runcmd(char *cmd) {
    char *argv[10];
    // 简单解析，假设输入是 "echo hello"
    argv[0] = "echo";
    argv[1] = "hello";
    argv[2] = 0;
    exec("/bin/echo", argv);   // 执行程序
    printf(2, "exec failed\n"); // 只有失败时执行
    exit(0);
}
```

#### 用户输入：

- 用户在 shell 中输入：echo hello。

#### 步骤分解：

1. **读取输入**：
    - getcmd 读取 "echo hello"，存入 buf。  
2. **调用 fork**：
    - 父进程（假设 PID = 100）调用 fork()。
    - 创建子进程（PID = 101），复制父进程的内存。
    - 父进程：fork() 返回 101，进入 wait。
    - 子进程：fork() 返回 0，进入 runcmd。
3. **子进程执行 runcmd**：
    - runcmd 解析 "echo hello"：  
        runcmd 解析 “echo hello”：
        - argv[0] = "echo"（程序名）。
        - argv[1] = "hello"（参数）。  
        - argv[2] = 0（结束）。
    - 调用 exec("/bin/echo", argv)。  
        调用 exec（“/bin/echo”， argv）。
4. **执行 exec**：
    - 子进程的内存被 /bin/echo 的 ELF 文件替换。
    - /bin/echo 运行，打印 "hello"。
    - /bin/echo 调用 exit(0) 退出。
5. **父进程等待**：
    - 子进程退出后，wait 返回 101。
    - 父进程继续循环，等待下一次输入。

## **为什么 fork 和 exec 分开？**

答：分开是为了灵活性，尤其是在 I/O 重定向中。

#### 示例（I/O 重定向）：

假设输入是 echo hello > file：

```
void runcmd(char *cmd) {
    char *argv[10];
    argv[0] = "echo";
    argv[1] = "hello";
    argv[2] = 0;
    if (fork() == 0) {              // 子进程
        close(1);                   // 关闭标准输出
        open("file", O_WRONLY | O_CREATE); // 重定向到文件
        exec("/bin/echo", argv);    // 执行 echo
        exit(0);
    }
    wait((int *) 0);                // 父进程等待
}
```

- **过程**：
    - fork 创建子进程。
    - 子进程修改文件描述符（重定向输出到 "file"）。
    - exec 运行 echo，输出到文件而不是终端。
- **结果**：hello 写入 "file"，终端无输出。
- 分离的好处：fork 后可以在子进程中调整环境（如重定向），然后再用 exec 执行。

---

## **fork 的优化：Copy-on-Write**

#### 问题：

- fork 复制整个内存后，exec 立即替换，似乎浪费资源。
- 答：Xv6 使用 **Copy-on-Write（写时复制）** 优化。

#### 概念：

- fork 不立即复制内存，而是让父子进程共享内存页面，标记为只读。
- 当任一进程修改内存时，触发复制，分配新页面。

#### 示例：

- 父进程内存：1MB。
- fork：  
    - 子进程共享这 1MB，只读。
    - 未修改时，无复制。
- 子进程调用 exec：
    - 替换内存前，几乎不占用额外空间。
- **优化效果**：减少内存浪费，提高效率


**sbrk 动态扩展**

在运行时需要更多内存的进程(可能是`malloc`)可以调用 `sbrk(n)`将其数据内存增加n个字节; `sbrk`返回新内存的位置。

### 类比理解

- **Shell 像餐厅服务员**：
    - 用户点菜（输入 "echo hello"）。
    - 服务员（父进程）叫助手（fork 创建子进程）。
    - 助手换上厨师制服（exec 替换为 echo）。
    - 服务员等待（wait），助手做菜（打印 "hello"）。
- **内存像房间**：
    - fork 复制房间（共享但只读）。
    - exec 装修新房间。
    - sbrk 扩展房间面积。

