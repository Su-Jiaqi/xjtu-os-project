#ifndef MINI_EXT2_FS_H
#define MINI_EXT2_FS_H

#include <stdint.h>
#include <stdio.h>
#include "layout.h"
#include "errors.h"
#include "util.h"

// 文件类型（目录项里）
#define FT_REG  1
#define FT_DIR  2

// 简化的 mode（带类型位）
#define MODE_FILE 0100644
#define MODE_DIR  0040755

// --------- 结构体 ----------
typedef struct {
    uint32_t magic;
    uint32_t block_size;
    uint32_t blocks_count;
    uint32_t inodes_count;
    uint32_t free_blocks;
    uint32_t free_inodes;
    uint32_t first_data_block;
    uint32_t inode_table_start;
    uint32_t block_bitmap_blk;
    uint32_t inode_bitmap_blk;
    uint32_t root_ino;
    uint64_t mount_time;
    uint64_t write_time;
} superblock_t;

typedef struct {
    uint32_t block_bitmap;
    uint32_t inode_bitmap;
    uint32_t inode_table;
    uint32_t free_blocks_count;
    uint32_t free_inodes_count;
} group_desc_t;

typedef struct {
    uint16_t mode;
    uint16_t uid;
    uint32_t size;
    uint32_t atime, mtime, ctime;
    uint16_t gid;
    uint16_t links;
    uint32_t blocks;
    uint32_t direct[NDIRECT];
    uint32_t indirect1;     // 单级间接表块号（0=无）
    uint8_t  _reserve[56];  // 填充到 128B
} inode_t;

typedef struct {
    uint32_t ino;
    uint16_t reclen;        // 固定 64
    uint8_t  file_type;     // FT_REG/FT_DIR
    char     name[NAME_MAX_LEN];
} dirent_t;

// --------- 全局状态 ----------
extern superblock_t g_sb;
extern group_desc_t g_gd;
extern FILE* g_dev;

#define MAX_OPEN 64
typedef struct {
    int used;
    uint32_t ino;
    uint32_t offset;
    int writable;
} ofile_t;

extern ofile_t g_ofile[MAX_OPEN];
extern uint32_t g_cwd;

// --------- 设备层 ----------
int dev_open(const char* path, const char* mode);
int dev_close();
int dev_read_block(void* buf, uint32_t blk_no);
int dev_write_block(const void* buf, uint32_t blk_no);

// --------- 位图/分配 ----------
int  bmap_test(uint32_t idx, int is_block);
int  bmap_set(uint32_t idx, int is_block, int val);
int  alloc_block();
void free_block(uint32_t blk);
int  alloc_inode();
void free_inode(uint32_t ino);

// --------- inode ----------
int read_inode(uint32_t ino, inode_t* out);
int write_inode(uint32_t ino, const inode_t* in);
int inode_truncate(uint32_t ino);

// --------- 目录/路径 ----------
int dir_lookup(uint32_t dir_ino, const char* name, uint32_t* out_ino);
int dir_add(uint32_t dir_ino, const char* name, uint8_t ftype, uint32_t child_ino);
int dir_remove(uint32_t dir_ino, const char* name);
int namei(const char* path, uint32_t* out_ino);

// --------- 文件 I/O ----------
int fs_open(const char* path, const char* mode);
int fs_close(int fd);
int fs_read(int fd, void* buf, uint32_t len);
int fs_write(int fd, const void* buf, uint32_t len);
int fs_seek(int fd, int32_t off);

// --------- FS 初始化 ----------
int fs_format();
int fs_mount(const char* img);

#endif
