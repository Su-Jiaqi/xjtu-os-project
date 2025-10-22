#ifndef MINI_EXT2_LAYOUT_H
#define MINI_EXT2_LAYOUT_H

// 基本参数
#define FS_MAGIC        0xEF53u
#define BLOCK_SIZE      512u
#define TOTAL_BLOCKS    4611u

// 磁盘布局（块号）
#define BLK_BOOT        0
#define BLK_SUPER       1
#define BLK_GDESC       2
#define BLK_BMAP        3
#define BLK_IMAP        4
#define BLK_ITBL_START  5
#define ITBL_BLOCKS     64
#define BLK_DATA_START  (BLK_ITBL_START + ITBL_BLOCKS) // 69

// Inode/目录项/指针数
#define MAX_INODES      256
#define INODE_SIZE      128
#define NDIRECT         10

// 目录项定长
#define NAME_MAX_LEN    56

#endif
