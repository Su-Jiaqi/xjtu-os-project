本实验包含三部分：  
1. 进程软中断通信（Signal）  
2. 管道通信（Pipe，有锁/无锁对比）  
3. 连续内存分配与回收（FF/BF/WF 算法）

---

## Part 1：软中断通信（Signal）

父进程安装信号处理函数，创建两个子进程等待信号。通过键盘或定时触发信号后，父进程依次向子进程发送用户信号。

```bash
cd part1
gcc -std=c99 -O2 -o softint softint.c
./softint

## Part 2：管道通信（Pipe）

父进程创建匿名管道，两个子进程并发写入数据。可选择无锁模式（数据交错）或有锁模式（互斥写入）。

```bash
cd part2
gcc -std=c99 -O2 -o pipe_communication pipe_communication.c
./pipe_communication --nolock   # 无锁模式
./pipe_communication --lock     # 有锁模式


## Part 3：内存分配与回收（FF/BF/WF）

模拟动态分区分配，支持首次/最佳/最坏适应算法，带最小碎片阈值 MIN_SLICE 与紧缩（Compaction）机制。

```bash
cd part3
gcc -std=c99 -O2 -o memsim memsim.c
./memsim