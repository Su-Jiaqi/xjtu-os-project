#ifndef MINI_EXT2_FS_H
#define MINI_EXT2_FS_H
#include <stdint.h>
#include <stdio.h>
#include <time.h>

// --- 常量与布局 ---
#define FS_MAGIC        0xEF53u
#define BLOCK_SIZE      512u
#define TOTAL_BLOCKS    4611u

#define BLK_BOOT        0
#define BLK_SUPER       1
#define BLK_GDESC       2
#define BLK_BMAP        3
#define BLK_IMAP        4
#define BLK_ITBL_START  5
#define ITBL_BLOCKS     64
#define BLK_DATA_START  (BLK_ITBL_START + ITBL_BLOCKS)  // 69

#define MAX_INODES      256
#define INODE_SIZE      128
#define NDIRECT         10

// 文件类型/目录项
#define FT_REG  1
#define FT_DIR  2

// 简化权限：类型位 + 9bit
#define MODE_FILE 0100644
#define MODE_DIR  0040755

// 错误码
#define FS_OK           0
#define FS_ERR         -1
#define FS_ENOENT      -2
#define FS_EEXIST      -3
#define FS_ENOSPC      -4
#define FS_ENOTDIR     -5
#define FS_EISDIR      -6
#define FS_EPERM       -7
#define FS_EBADF       -8

#define NAME_MAX_LEN 56

// --- 结构体 ---
typedef struct {
    uint32_t magic, block_size, blocks_count, inodes_count;
    uint32_t free_blocks, free_inodes;
    uint32_t first_data_block, inode_table_start;
    uint32_t block_bitmap_blk, inode_bitmap_blk;
    uint32_t root_ino;
    uint64_t mount_time, write_time;
} superblock_t;

typedef struct {
    uint32_t block_bitmap, inode_bitmap, inode_table;
    uint32_t free_blocks_count, free_inodes_count;
} group_desc_t;

typedef struct {
    uint16_t mode;          // 类型+权限
    uint16_t uid;           // 所有者
    uint32_t size;
    uint32_t atime, mtime, ctime;
    uint16_t gid;
    uint16_t links;
    uint32_t blocks;
    uint32_t direct[NDIRECT];
    uint32_t indirect1;     // 单级间接
    uint8_t  _reserve[56];
} inode_t;

typedef struct {
    uint32_t ino;
    uint16_t reclen;        // 固定 64
    uint8_t  file_type;     // FT_REG/FT_DIR
    char     name[NAME_MAX_LEN];
} dirent_t;

// --- 打开文件表 ---
#define MAX_OPEN 64
typedef struct {
    int used;
    uint32_t ino;
    uint32_t offset;
    int writable;
} ofile_t;

// --- 全局状态 ---
extern superblock_t g_sb;
extern group_desc_t g_gd;
extern FILE* g_dev;
extern ofile_t g_ofile[MAX_OPEN];
extern uint32_t g_cwd;

// 登录状态（教学版）
#define MAX_USER_LEN 16
extern int  g_uid;                 // 0=root；其它统一当作 1
extern char g_user[MAX_USER_LEN];  // 当前用户名

// --- 设备层 ---
int dev_open(const char* path, const char* mode);
int dev_close();
int dev_read_block(void* buf, uint32_t blk_no);
int dev_write_block(const void* buf, uint32_t blk_no);

// --- 位图/分配 ---
int  bmap_test(uint32_t idx, int is_block);
int  bmap_set(uint32_t idx, int is_block, int val);
int  alloc_block();
void free_block(uint32_t blk);
int  alloc_inode();
void free_inode(uint32_t ino);

// --- inode ---
int read_inode(uint32_t ino, inode_t* out);
int write_inode(uint32_t ino, const inode_t* in);
int inode_truncate(uint32_t ino);

// --- 目录/路径 ---
int dir_lookup(uint32_t dir_ino, const char* name, uint32_t* out_ino);
int dir_add(uint32_t dir_ino, const char* name, uint8_t ftype, uint32_t child_ino);
int dir_remove(uint32_t dir_ino, const char* name);
int namei(const char* path, uint32_t* out_ino);

// --- 文件 I/O ---
int fs_open(const char* path, const char* mode);
int fs_close(int fd);
int fs_read(int fd, void* buf, uint32_t len);
int fs_write(int fd, const void* buf, uint32_t len);
int fs_seek(int fd, int32_t off);

// 权限检查
// int perm_can_read(const inode_t* in, int uid);
// int perm_can_write(const inode_t* in, int uid);

// --- FS 初始化 ---
int fs_format();
int fs_mount(const char* img);

// --- 工具 ---
void ts_now(uint32_t* out);
void human_time(uint32_t t, char* out, size_t n);
void mode_to_str(uint16_t mode, char out[11]);

// --- 账号/口令（基于 /.users 文本文件） ---
int users_bootstrap(void);
int users_login(const char* user, const char* pass);
int users_add(const char* user, const char* pass);
int users_change_password(const char* name, const char* pass);

// 会话持久化：把 g_uid/g_user 保存到 /.session，跨进程恢复
int session_save(int uid, const char* user);
int session_load();  // 读取后设置 g_uid/g_user；不存在则保持 root

#endif
