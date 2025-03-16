#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

#define MAX_LINE 512 // 每行最大长度

// 读取一行输入，返回行的长度（不包括换行符）
int readline(char *buf, int max) {
    int i = 0;
    char c;
    while (i < max - 1) {
        if (read(0, &c, 1) <= 0) { // EOF 或错误
            if (i == 0) return -1; // 无数据
            break;
        }
        if (c == '\n') break; // 遇到换行符
        buf[i++] = c;
    }
    buf[i] = 0; // 字符串结束
    return i;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(2, "usage: xargs command [args...]\n");
        exit(1);
    }

    char line[MAX_LINE];
    char *cmd_argv[MAXARG]; // 参数数组，MAXARG 定义在 kernel/param.h
    int i;

    // 复制命令及其初始参数
    for (i = 1; i < argc; i++) {
        cmd_argv[i - 1] = argv[i];
    }

    // 逐行读取输入
    while (readline(line, MAX_LINE) >= 0) {
        // 将当前行作为额外参数
        cmd_argv[argc - 1] = line;
        cmd_argv[argc] = 0; // 参数列表以 NULL 结束

        // 创建子进程执行命令
        if (fork() == 0) {
            exec(cmd_argv[0], cmd_argv);
            fprintf(2, "exec %s failed\n", cmd_argv[0]);
            exit(1);
        }
        wait(0); // 等待子进程完成
    }

    exit(0);
}
