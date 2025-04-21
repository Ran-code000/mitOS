XV6的源代码位于 **_kernel/_** 子目录中，源代码按照模块化的概念划分为多个文件，图2.2列出了这些文件，模块间的接口都被定义在了**_def.h_**（**_kernel/defs.h_**）。

| **文件**              | **描述**                                      |
| ------------------- | ------------------------------------------- |
| **_bio.c_**         | 文件系统的磁盘块缓存                                  |
| **_console.c_**     | 连接到用户的键盘和屏幕                                 |
| **_entry.S_**       | 首次启动指令                                      |
| **_exec.c_**        | `exec()`系统调用                                |
| **_file.c_**        | 文件描述符支持                                     |
| **_fs.c_**          | 文件系统                                        |
| **_kalloc.c_**      | 物理页面分配器                                     |
| **_kernelvec.S_**   | 处理来自内核的陷入指令以及计时器中断                          |
| **_log.c_**         | 文件系统日志记录以及崩溃修复                              |
| **_main.c_**        | 在启动过程中控制其他模块初始化                             |
| **_pipe.c_**        | 管道                                          |
| **_plic.c_**        | RISC-V中断控制器                                 |
| **_printf.c_**      | 格式化输出到控制台                                   |
| **_proc.c_**        | 进程和调度                                       |
| **_sleeplock.c_**   | Locks that yield the CPU                    |
| **_spinlock.c_**    | Locks that don’t yield the CPU.             |
| **_start.c_**       | 早期机器模式启动代码                                  |
| **_string.c_**      | 字符串和字节数组库                                   |
| **_swtch.c_**       | 线程切换                                        |
| **_syscall.c_**     | Dispatch system calls to handling function. |
| **_sysfile.c_**     | 文件相关的系统调用                                   |
| **_sysproc.c_**     | 进程相关的系统调用                                   |
| **_trampoline.S_**  | 用于在用户和内核之间切换的汇编代码                           |
| **_trap.c_**        | 对陷入指令和中断进行处理并返回的C代码                         |
| **_uart.c_**        | 串口控制台设备驱动程序                                 |
| **_virtio_disk.c_** | 磁盘设备驱动程序                                    |
| **_vm.c_**          | 管理页表和地址空间                                   |
不认真看做lab2就后悔死了！！