# README – Project 1 实验报告

## 一、项目简介

本项目基于 openEuler Linux 环境，旨在通过编写和运行多进程、多线程及同步互斥相关程序，理解操作系统中并发执行的基本机制。

实验主要包括三部分：

- **进程实验**：掌握 `fork()`、`wait()`、`system()`、`exec()` 等系统调用的使用，理解父子进程的关系及独立地址空间。

- **线程实验**：通过共享变量演示竞态条件的产生与解决，利用信号量 (Semaphore) 实现互斥同步。

- **自旋锁实验**：实现自定义自旋锁结构，验证忙等待机制的正确性与互斥效果。

实验帮助掌握 Linux 下的基本并发编程原理，为后续操作系统课程设计与系统级开发打下基础。

## 二、编译与运行方法

### （1）进程实验

```bash
# 编译
gcc fork_example.c -o fork_example
gcc system_exec.c -o system_exec
gcc system_call.c -o system_call

# 运行
./fork_example
./system_exec
```

### （2）线程实验

```bash
# 编译
gcc step1_race.c -o step1_race -lpthread
gcc step2_semaphore.c -o step2_semaphore -lpthread
gcc step3_system_in_thread.c -o step3_system_in_thread -lpthread
gcc step3_exec_in_thread.c exec_helper.c -o step3_exec_in_thread -lpthread

# 运行
./step1_race
./step2_semaphore
./step3_system_in_thread
./step3_exec_in_thread
```

### （3）自旋锁实验

```bash
# 编译与运行
gcc spinlock.c -o spinlock -lpthread
./spinlock
```

## 三、文件结构

```
project1/
├── part1/
│   ├── 1-1           # 可执行文件
│   ├── 1-1.c
│   ├── 1-2
│   ├── 1-2.c
│   ├── 1-3
│   ├── 1-3.c
│   ├── 1-4
│   ├── 1-4.c
│   ├── 1-5-1
│   ├── 1-5-1.c
│   ├── 1-5-2
│   ├── 1-5-2.c
│   ├── system_call
│   └── system_call.c
├── part2/
│   ├── exec_helper
│   ├── exec_helper.c
│   ├── step1_race
│   ├── step1_race.c
│   ├── step2_semaphore
│   ├── step2_semaphore.c
│   ├── step3_exec_in_thread
│   ├── step3_exec_in_thread.c
│   ├── step3_system_in_thread
│   └── step3_system_in_thread.c
├── part3/
│   ├── spinlock
│   └── spinlock.c
└── README.md
```
