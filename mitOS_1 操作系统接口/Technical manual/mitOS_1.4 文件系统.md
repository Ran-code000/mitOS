
### Xv6 文件系统概述

#### 基本结构：

- **数据文件**：未解释的字节数组。
- **目录**：包含文件名和 inode 引用的树状结构。
- **根目录**：以 / 开头，所有路径从这里开始。
- **当前工作目录**：每个进程有一个当前目录，可通过 chdir 修改。

#### 路径解析：

- **绝对路径**：以 / 开头，从根目录解析（如 /a/b/c）。
- **相对路径**：从当前工作目录解析（如 b/c）。

```
#include "types.h"
#include "user.h"
#include "fcntl.h"

int main() {
    chdir("/a");         // 当前目录变为 /a
    chdir("b");          // 当前目录变为 /a/b
    open("c", O_RDONLY); // 打开 /a/b/c
    open("/a/b/c", O_RDONLY); // 同上，但用绝对路径
    exit(0);
}
```

---

### 创建文件和目录的系统调用

#### 系统调用：

1. **mkdir**：创建目录。
2. **open（带 O_CREATE）**：创建数据文件。
3. **mknod**：创建设备文件。

```
#include "types.h"
#include "user.h"
#include "fcntl.h"

int main() {
    mkdir("/dir");                    // 创建目录 /dir
    int fd = open("/dir/file", O_CREATE | O_WRONLY); // 创建文件 /dir/file
    close(fd);
    mknod("/console", 1, 1);          // 创建设备文件 /console
    exit(0);
}
```
- mkdir("/dir")：在根目录下创建目录 dir。
- open("/dir/file", O_CREATE | O_WRONLY)：在 /dir 下创建文件 file，返回文件描述符。
- close(fd)：关闭描述符。
- mknod("/console", 1, 1)：创建设备文件，主设备号 1，次设备号 1，关联内核设备（如控制台）。

#### 设备文件：

- **mknod** 是一个系统调用，用于创建特殊文件（设备文件），引用内核中的设备
```
int mknod(const char *path, int major, int minor);
```
- path：设备文件的路径和名称。
- major（主设备号）：标识设备类型（如控制台、磁盘）。
- minor（次设备号）：标识具体设备实例（如控制台 1、磁盘分区 2）。

#### 作用：

- 创建的设备文件是一个特殊的 inode，其类型为 T_DEVICE。
- 当进程打开设备文件并调用 read 或 write 时，内核根据主、次设备号调用对应的设备驱动程序，而不是文件系统的存储逻辑。


```
#include "types.h"
#include "user.h"
#include "fcntl.h"

int main() {
    mknod("/console", 1, 1);          // 创建设备文件 /console
    int fd = open("/console", O_RDWR); // 打开设备文件
    write(fd, "hello\n", 6);          // 写入设备
    close(fd);
    exit(0);
}
```

**过程**：

1. **mknod("/console", 1, 1)**：  
    - 在根目录下创建文件 /console。
    - inode 类型设为 T_DEVICE，记录主设备号 1（假设为控制台）和次设备号 1。
2. **open("/console", O_RDWR)**：  
    - 返回文件描述符（例如 fd=3），关联到 /console 的 inode。
3. **write(fd, "hello\n", 6)**：  
    - 内核识别 fd 指向设备文件，根据主、次设备号调用控制台驱动。
    - 输出 "hello" 到控制台，而不是磁盘文件。
4. **close(fd)**：  
    - 释放描述符。

#### 与普通文件区别：

- **普通文件**：read 和 write 操作磁盘上的数据块。
- **设备文件**：read 和 write 调用设备驱动（如控制台、键盘），不涉及文件系统存储。

#### 主、次设备号：

- **主设备号**：标识设备类别（例如，1 表示控制台设备）。
- **次设备号**：区分同一类别中的具体实例（例如，1 表示第一个控制台）。
- 组合唯一标识内核中的设备驱动。


### 类比理解

- **设备文件（mknod）**：
    - 像电话号码（/console），拨号（open）后连接到设备（主、次设备号），通话（read/write）直接与设备交互。
---

### 文件名与 Inode 的关系

#### 概念：

- **文件**：由 inode 表示，包含数据和元数据。

  Inode（索引节点）：
```
struct stat {
    int dev;     // 文件系统所在的磁盘设备
    uint ino;    // 唯一标识 inode
    short type;  // 类型（T_FILE, T_DIR, T_DEVICE），区分普通文件、目录、设备
    short nlink; // 指向此 inode 的链接数量
    uint64 size; // 文件大小（字节）
};
```

- #### 链接（Link）：文件名到 inode 的映射
- **硬链接**：通过 link 创建，直接引用同一 inode。
- **符号链接**（Xv6 不支持）：指向另一个文件名

- **删除**：只有当 nlink=0 且无打开描述符时，文件才真正释放

#### 概念(人话)：

- **文件名**：用户看到的标识符，存在于目录中。
- **文件本身**：底层数据实体，由 inode 表示。
- 一个文件（inode）可以有多个名字（通过链接），文件名只是指向 inode 的引用。

#### 类比：

- 想象一本书：
    - **书本身**：内容和元数据（作者、页数），类似 inode。
    - **书名**：图书馆目录卡上的名字，类似文件名。
    - 一本书可以有多张目录卡（多个名字），但内容只有一份。

```
#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "stat.h"

int main() {
    int fd = open("a", O_CREATE | O_WRONLY); // 创建文件 a
    write(fd, "data", 4);
    link("a", "b");                          // 创建链接 b
    struct stat st1, st2;
    fstat(fd, &st1);                         // 检查 a 的 inode
    close(fd);
    fd = open("b", O_RDONLY);
    fstat(fd, &st2);                         // 检查 b 的 inode
    printf(1, "a: ino=%d, nlink=%d\n", st1.ino, st1.nlink);
    printf(1, "b: ino=%d, nlink=%d\n", st2.ino, st2.nlink);
    char buf[5];
    read(fd, buf, 4);
    buf[4] = '\0';
    printf(1, "b content: %s\n", buf);
    close(fd);
    exit(0);
}
```

**过程**：

1. **open("a", O_CREATE | O_WRONLY)**：  
    - 创建文件 a，分配 inode（假设 ino=10），nlink=1。
2. **write(fd, "data", 4)**：  
    - 写入 "data" 到 inode 的数据块。
3. **link("a", "b")**：  
    - 在当前目录创建条目 b，指向 ino=10，nlink 增至 2。
4. **fstat**：  **fstat** 中：
    - a 和 b 的 inode 号相同（ino=10），nlink=2。
5. **open("b", O_RDONLY) 和 read**：  
    - 从 b 读取，得到 "data"，因为底层是同一 inode


#### 删除链接：

- **unlink**：移除文件名，只有当 **nlink=0 且无描述符引用**时，释放 inode
```
unlink("a"); // nlink 减至 1，文件仍可通过 b 访问
```


创建临时无名文件

```
#include "types.h"
#include "user.h"
#include "fcntl.h"

int main() {
    int fd = open("/tmp/xyz", O_CREATE | O_RDWR); // 创建文件
    unlink("/tmp/xyz");                           // 立即删除名字
    write(fd, "temp data", 9);                    // 仍可写入
    close(fd);                                    // 关闭后清理
    exit(0);
}
```

**过程**：

- open：创建 /tmp/xyz，分配 inode。
- unlink：移除名字，nlink=0，但 fd 仍引用 inode。
- write：操作有效，因为 inode 未释放。
- close：无引用，inode 和磁盘空间释放

---

### 为什么 cd 内置于 Shell？

#### 问题：

- 如果 cd 作为普通命令运行，会影响子进程，而非 shell 本身

示例代码（**错误**方式）：
```
#include "types.h"
#include "user.h"

int main() {
    if (fork() == 0) {
        chdir("/a");         // 子进程更改目录
        exit(0);
    }
    wait((int *) 0);
    char buf[32];
    getcwd(buf, sizeof(buf)); // 获取当前目录
    printf(1, "cwd: %s\n", buf); // shell 的目录未变
    exit(0);
}
```
**输出**（假设初始目录为 /）：/
**原因**：

- 子进程的 chdir("/a") 只修改子进程的当前目录。
- 父进程（shell）的目录保持不变。


正确实现（内置于 shell）：
**Shell 代码（伪代码，参考 user/sh.c）**：
```
void runcmd(char *cmd) {
    if (strcmp(cmd, "cd /a") == 0) {
        chdir("/a");         // 直接更改 shell 的目录
    } else {
        if (fork() == 0) {
            exec(...);       // 其他命令在子进程运行
        }
        wait((int *) 0);
    }
}

int main() {
    char buf[32];
    while (1) {
        printf(1, "$ ");
        gets(buf, sizeof(buf));
        runcmd(buf);
    }
}
```
- **过程**：
    - 输入 cd /a：
        - chdir("/a") 直接修改 shell 的当前目录。
    - 输入 ls：
        - fork 和 exec 在子进程运行，不影响 shell。

#### 为什么内置？

- cd 必须改变 shell 的状态（当前目录），外部命令只能影响子进程


### 类比理解

- **文件系统像图书馆**：
    - **文件**：书（数据）。
    - **目录**：书架（组织结构）。
    - **Inode**：书的编号。
    - **Link**：书的多本索引卡。
- **管道 vs Shell**：
    - cd 像调整你的座位（shell 状态），不能让别人替你坐