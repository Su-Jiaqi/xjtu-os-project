/* ==========================================================
 *  图 2.2 管道通信实验完整程序（运行参数控制锁）
 *
 *  功能：
 *    两个子进程各向匿名管道写入 2000 个字符，
 *    父进程从管道读取 4000 个字符并显示。
 *
 *  用法：
 *    ./pipe_communication --lock     有锁（互斥写）
 *    ./pipe_communication --nolock   无锁（默认）
 *
 * ========================================================== */
 #include <unistd.h>
 #include <signal.h>
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <sys/wait.h>
 #include <fcntl.h>
 #include <errno.h>
 
 #define WRITE_COUNT 2000
 #define TOTAL_READ  (WRITE_COUNT * 2)
 
 int pid1, pid2;
 
 int main(int argc, char *argv[]) {
     int use_lock = 0; // 默认无锁
     if (argc >= 2 && strcmp(argv[1], "--lock") == 0)
         use_lock = 1;
 
     int fd[2];
     char InPipe[5000];       // 缓冲区足够大
     char c1 = '1', c2 = '2'; // 两个子进程写入的字符
 
     // 创建管道
     if (pipe(fd) == -1) {
         perror("pipe");
         exit(1);
     }
 
     // ---------- 创建子进程 1 ----------
     while ((pid1 = fork()) == -1);
     if (pid1 == 0) {
         close(fd[0]); // 子进程1只写，关闭读端
 
         if (use_lock)
             lockf(fd[1], F_LOCK, 0); // 锁定管道写入端
 
         for (int i = 0; i < WRITE_COUNT; i++) {
             if (write(fd[1], &c1, 1) != 1) {
                 perror("write child1");
                 exit(1);
             }
         }
 
         sleep(5); // 等待读进程读取
 
         if (use_lock)
             lockf(fd[1], F_ULOCK, 0); // 解除锁定
 
         close(fd[1]);
         exit(0); // 结束子进程1
     } else {
         // ---------- 创建子进程 2 ----------
         while ((pid2 = fork()) == -1);
         if (pid2 == 0) {
             close(fd[0]); // 子进程2只写，关闭读端
 
             if (use_lock)
                 lockf(fd[1], F_LOCK, 0);
 
             for (int i = 0; i < WRITE_COUNT; i++) {
                 if (write(fd[1], &c2, 1) != 1) {
                     perror("write child2");
                     exit(1);
                 }
             }
 
             sleep(5);
 
             if (use_lock)
                 lockf(fd[1], F_ULOCK, 0);
 
             close(fd[1]);
             exit(0); // 结束子进程2
         } else {
             // ---------- 父进程 ----------
             close(fd[1]); // 父进程只读，关闭写端
 
             wait(NULL); // 等待子进程1结束
             wait(NULL); // 等待子进程2结束
 
             // 从管道读取4000字符
             int nread = 0;
             while (nread < TOTAL_READ) {
                 ssize_t n = read(fd[0], InPipe + nread, TOTAL_READ - nread);
                 if (n < 0) {
                     if (errno == EINTR) continue;
                     perror("read parent");
                     exit(1);
                 }
                 if (n == 0) break;
                 nread += n;
             }
             InPipe[nread] = '\0';
 
             // 输出统计信息
             int count1 = 0, count2 = 0, switches = 0;
             for (int i = 0; i < nread; i++) {
                 if (InPipe[i] == '1') count1++;
                 else if (InPipe[i] == '2') count2++;
                 if (i > 0 && InPipe[i] != InPipe[i-1]) switches++;
             }
 
             printf("==== 管道通信实验结果 ====\n");
             printf("模式: %s\n", use_lock ? "有锁（互斥写）" : "无锁（并发写）");
             printf("读取总字节: %d, '1'=%d, '2'=%d, switches=%d\n",
                    nread, count1, count2, switches);
             printf("前120字符: %.120s%s\n", InPipe, (nread > 120 ? "..." : ""));
             printf("后120字符: %.120s%s\n",
                    nread > 120 ? InPipe + nread - 120 : InPipe,
                    nread > 120 ? "..." : "");
 
             close(fd[0]);
             exit(0);
         }
     }
 }
 