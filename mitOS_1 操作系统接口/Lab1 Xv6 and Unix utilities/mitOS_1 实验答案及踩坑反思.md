
# Lab1: Xv6 and Unix utilities
---

## 启动xv6(难度：Easy)

/


---

## sleep(难度：Easy)


答案：
```
#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char const *argv[])
{
  if (argc < 2) {
    fprintf(2, "usage: sleep <time>\n");
    exit(1);
  }
  sleep(atoi(argv[1]));
  exit(0);
}

```

踩坑：头文件的顺序很重要

头文件中定义的宏可能会被后续头文件使用。如果顺序颠倒，可能导致未定义的宏引发编译错误

```
#include "kernel/types.h"
#include "user/user.h"
```

这两句顺序若颠倒会出现编译错误，由于 uint 未定义，编译器误以为 uint 是参数名而非类型。


## pingpong（难度：Easy）


我的：
```
#include "kernel/types.h"
#include "user/user.h"

int main(){
    int p1[2];
    int p2[2];
    pipe(p1); //子写父读
    pipe(p2); //子读父写
    if(fork() == 0){
        close(p2[1]);
        char buf[10];
        read(p2[0], buf, 4);
        fprintf(1, "%d: receive %s\n", getpid(), buf);
        close(p2[0]);

        close(p1[0]);
        write(p1[1], "pong", 4);
        close(p1[1]);
        exit(0);
    }else{
        close(p2[0]);
        write(p2[1], "ping", 4);
        close(p2[1]);

        close(p1[1]);
        char buf[10];
        read(p1[0], buf, 4);
        fprintf(1, "%d: received %s\n", getpid(), buf);
        close(p1[0]);
        exit(0);
    }
}

```

标准答案：
```
#include "kernel/types.h"
#include "user/user.h"

#define RD 0 //pipe的read端
#define WR 1 //pipe的write端

int main(int argc, char const *argv[]) {
    char buf = 'P'; //用于传送的字节

    int fd_c2p[2]; //子进程->父进程
    int fd_p2c[2]; //父进程->子进程
    pipe(fd_c2p);
    pipe(fd_p2c);

    int pid = fork();
    int exit_status = 0;

    if (pid < 0) {
        fprintf(2, "fork() error!\n");
        close(fd_c2p[RD]);
        close(fd_c2p[WR]);
        close(fd_p2c[RD]);
        close(fd_p2c[WR]);
        exit(1);
    } else if (pid == 0) { //子进程
        close(fd_p2c[WR]);
        close(fd_c2p[RD]);

        if (read(fd_p2c[RD], &buf, sizeof(char)) != sizeof(char)) {
            fprintf(2, "child read() error!\n");
            exit_status = 1; //标记出错
        } else {
            fprintf(1, "%d: received ping\n", getpid());
        }

        if (write(fd_c2p[WR], &buf, sizeof(char)) != sizeof(char)) {
            fprintf(2, "child write() error!\n");
            exit_status = 1;
        }

        close(fd_p2c[RD]);
        close(fd_c2p[WR]);

        exit(exit_status);
    } else { //父进程
        close(fd_p2c[RD]);
        close(fd_c2p[WR]);

        if (write(fd_p2c[WR], &buf, sizeof(char)) != sizeof(char)) {
            fprintf(2, "parent write() error!\n");
            exit_status = 1;
        }

        if (read(fd_c2p[RD], &buf, sizeof(char)) != sizeof(char)) {
            fprintf(2, "parent read() error!\n");
            exit_status = 1; //标记出错
        } else {
            fprintf(1, "%d: received pong\n", getpid());
        }

        close(fd_p2c[WR]);
        close(fd_c2p[RD]);

        exit(exit_status);
    }
}

```

## Primes(素数，难度：Moderate/Hard)

初版（错误： timeout）
```
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define re 0
#define wr 1

int main(){
    int p[2];
    int p2[2];
    int p3[2];
    int p5[2];
    pipe(p);
    pipe(p2);
    pipe(p3);
    pipe(p5);

    if(fork()==0){
        close(p[re]);
        for(int i = 2; i <= 35; i++){
            write(p[wr], &i, 4);
        }
        close(p[wr]);

        exit(0);
    }
    if(fork()==0){
        int num;
        close(p[wr]);
        read(p[re], &num, 4);
        fprintf(1, "prime %d\n", num);

        close(p2[re]);
        int t;
        while(read(p[re], &t, 4)){
            if(t % num != 0){
                write(p2[wr], &t, 4);
            }
        }
        close(p[re]);
        close(p2[wr]);
        exit(0);
    }
    if(fork()==0){
        int numm;
        close(p2[wr]);
        read(p2[re], &numm, 4);
        fprintf(1, "prime %d\n", numm);

        close(p3[re]);
        int tt;
        while(read(p2[re], &tt, 4)){
            if(tt % numm != 0){
                write(p3[wr], &tt, 4);
            }
        }
        close(p3[wr]);
        close(p2[re]);
        exit(0);
    }
    if(fork()==0){
        int nummm;
        close(p3[wr]);
        read(p3[re], &nummm, 4);
        fprintf(1, "prime %d\n", nummm);

        close(p5[re]);
        int ttt;
        while(read(p3[re], &ttt, 4)){
            if(ttt % nummm != 0){
                write(p5[wr], &ttt, 4);
            }
        }
        close(p5[wr]);
        close(p3[re]);
        exit(0);
    }
    if(fork()==0){
        close(p5[wr]);
        int res;
        while(read(p5[re], &res, 4)){
            fprintf(1, "prime %d\n", res);
        }
        close(p5[re]);
        exit(0);
    }

    close(p[re]);
    close(p[wr]);
    close(p2[re]);
    close(p2[wr]);
    close(p3[re]);
    close(p3[wr]);
    close(p5[re]);
    close(p5[wr]);

    wait(0);
    wait(0);
    wait(0);
    wait(0);
    wait(0);

    exit(0);
}
```

标准答案：
```
#include "kernel/types.h"
#include "user/user.h"

#define RD 0
#define WR 1

const uint INT_LEN = sizeof(int);

/**
 * @brief 读取左邻居的第一个数据
 * @param lpipe 左邻居的管道符
 * @param pfirst 用于存储第一个数据的地址
 * @return 如果没有数据返回-1,有数据返回0
 */
int lpipe_first_data(int lpipe[2], int *dst)
{
  if (read(lpipe[RD], dst, sizeof(int)) == sizeof(int)) {
    printf("prime %d\n", *dst);
    return 0;
  }
  return -1;
}

/**
 * @brief 读取左邻居的数据，将不能被first整除的写入右邻居
 * @param lpipe 左邻居的管道符
 * @param rpipe 右邻居的管道符
 * @param first 左邻居的第一个数据
 */
void transmit_data(int lpipe[2], int rpipe[2], int first)
{
  int data;
  // 从左管道读取数据
  while (read(lpipe[RD], &data, sizeof(int)) == sizeof(int)) {
    // 将无法整除的数据传递入右管道
    if (data % first)
      write(rpipe[WR], &data, sizeof(int));
  }
  close(lpipe[RD]);
  close(rpipe[WR]);
}

/**
 * @brief 寻找素数
 * @param lpipe 左邻居管道
 */
void primes(int lpipe[2])
{
  close(lpipe[WR]);
  int first;
  if (lpipe_first_data(lpipe, &first) == 0) {
    int p[2];
    pipe(p); // 当前的管道
    transmit_data(lpipe, p, first);

    if (fork() == 0) {
      primes(p);    // 递归的思想，但这将在一个新的进程中调用
    } else {
      close(p[RD]);
      wait(0);
    }
  }
  exit(0);
}

int main(int argc, char const *argv[])
{
  int p[2];
  pipe(p);

  for (int i = 2; i <= 35; ++i) //写入初始数据
    write(p[WR], &i, INT_LEN);

  if (fork() == 0) {
    primes(p);
  } else {
    close(p[WR]);
    close(p[RD]);
    wait(0);
  }

  exit(0);
}

```
- primes 函数使用递归思想，但递归是通过 fork() 在新进程中实现的，而不是传统的函数调用栈。
- 每次调用 primes，一个新子进程接管新管道（p），父进程等待子进程完成。


## find（难度：Moderate）

find 实现非常类似于 ls

ls.c:
```
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  return buf;
}

void
ls(char *path)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_FILE:
    printf("%s %d %d %l\n", fmtname(path), st.type, st.ino, st.size);
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("ls: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf("ls: cannot stat %s\n", buf);
        continue;
      }
      printf("%s %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  int i;

  if(argc < 2){
    ls(".");
    exit(0);
  }
  for(i=1; i<argc; i++)
    ls(argv[i]);
  exit(0);
}
```
find.c
```
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  return buf;
}

void
find(char *path, char* filename)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      fprintf(2, "find: path too long\n");
      return;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        fprintf(2, "find: cannot stat %s\n", buf);
        continue;
      }
  //重点改动部分！！
        if (st.type == T_DIR && strcmp(p, ".") != 0 && strcmp(p, "..") != 0)
                find(buf, filename);
        else if (strcmp(filename, p) == 0)
                fprintf(1, "%s\n", buf);

  }
  close(fd);
}

int
main(int argc, char *argv[])
{

  if(argc < 3){
        fprintf(2, "usage: find <directory> <filename>\n");
    exit(1);
  }
  find(argv[1], argv[2]);
  exit(0);
}
```

## xargs（难度：Moderate）

答案一
```
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
```
答案二：
```
#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

#define MAXSZ 512
// 有限状态自动机状态定义
enum state {
  S_WAIT,         // 等待参数输入，此状态为初始状态或当前字符为空格
  S_ARG,          // 参数内
  S_ARG_END,      // 参数结束
  S_ARG_LINE_END, // 左侧有参数的换行，例如"arg\n"
  S_LINE_END,     // 左侧为空格的换行，例如"arg  \n""
  S_END           // 结束，EOF
};

// 字符类型定义
enum char_type {
  C_SPACE,
  C_CHAR,
  C_LINE_END
};

/**
 * @brief 获取字符类型
 *
 * @param c 待判定的字符
 * @return enum char_type 字符类型
 */
enum char_type get_char_type(char c)
{
  switch (c) {
  case ' ':
    return C_SPACE;
  case '\n':
    return C_LINE_END;
  default:
    return C_CHAR;
  }
}

/**
 * @brief 状态转换
 *
 * @param cur 当前的状态
 * @param ct 将要读取的字符
 * @return enum state 转换后的状态
 */
enum state transform_state(enum state cur, enum char_type ct)
{
  switch (cur) {
  case S_WAIT:
    if (ct == C_SPACE)    return S_WAIT;
    if (ct == C_LINE_END) return S_LINE_END;
    if (ct == C_CHAR)     return S_ARG;
    break;
  case S_ARG:
    if (ct == C_SPACE)    return S_ARG_END;
    if (ct == C_LINE_END) return S_ARG_LINE_END;
    if (ct == C_CHAR)     return S_ARG;
    break;
  case S_ARG_END:
  case S_ARG_LINE_END:
  case S_LINE_END:
    if (ct == C_SPACE)    return S_WAIT;
    if (ct == C_LINE_END) return S_LINE_END;
    if (ct == C_CHAR)     return S_ARG;
    break;
  default:
    break;
  }
  return S_END;
}


/**
 * @brief 将参数列表后面的元素全部置为空
 *        用于换行时，重新赋予参数
 *
 * @param x_argv 参数指针数组
 * @param beg 要清空的起始下标
 */
void clearArgv(char *x_argv[MAXARG], int beg)
{
  for (int i = beg; i < MAXARG; ++i)
    x_argv[i] = 0;
}

int main(int argc, char *argv[])
{
  if (argc - 1 >= MAXARG) {
    fprintf(2, "xargs: too many arguments.\n");
    exit(1);
  }
  char lines[MAXSZ];
  char *p = lines;
  char *x_argv[MAXARG] = {0}; // 参数指针数组，全部初始化为空指针

  // 存储原有的参数
  for (int i = 1; i < argc; ++i) {
    x_argv[i - 1] = argv[i];
  }
  int arg_beg = 0;          // 参数起始下标
  int arg_end = 0;          // 参数结束下标
  int arg_cnt = argc - 1;   // 当前参数索引
  enum state st = S_WAIT;   // 起始状态置为S_WAIT

  while (st != S_END) {
    // 读取为空则退出
    if (read(0, p, sizeof(char)) != sizeof(char)) {
      st = S_END;
    } else {
      st = transform_state(st, get_char_type(*p));
    }

    if (++arg_end >= MAXSZ) {
      fprintf(2, "xargs: arguments too long.\n");
      exit(1);
    }

    switch (st) {
    case S_WAIT:          // 这种情况下只需要让参数起始指针前移
      ++arg_beg;
      break;
    case S_ARG_END:       // 参数结束，将参数地址存入x_argv数组中
      x_argv[arg_cnt++] = &lines[arg_beg];
      arg_beg = arg_end;
      *p = '\0';          // 替换为字符串结束符
      break;
    case S_ARG_LINE_END:  // 将参数地址存入x_argv数组中同时执行指令
      x_argv[arg_cnt++] = &lines[arg_beg];
      // 不加break，因为后续处理同S_LINE_END
    case S_LINE_END:      // 行结束，则为当前行执行指令
      arg_beg = arg_end;
      *p = '\0';
      if (fork() == 0) {
        exec(argv[1], x_argv);
      }
      arg_cnt = argc - 1;
      clearArgv(x_argv, arg_cnt);
      wait(0);
      break;
    default:
      break;
    }

    ++p;    // 下一个字符的存储位置后移
  }
  exit(0);
}

```