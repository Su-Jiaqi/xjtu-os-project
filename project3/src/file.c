// src/file.c — 带权限校验与单级间接的数据块映射
#include <string.h>
#include <stdlib.h>
#include "fs.h"

// ===== 引用全局状态 =====
extern int g_uid;       // 当前登录用户（root=0）
extern uint32_t g_cwd;  // 当前目录（用于相对路径解析）

// ======= 权限检查（简化版） =======
// 约定：root(uid=0) 拥有一切权限；owner 看 0400/0200；others 看 0004/0002
static int perm_can_read(const inode_t* in, int uid){
    if(uid == 0) return 1;
    if(in->uid == (uint16_t)uid) return (in->mode & 0400) != 0;
    return (in->mode & 0004) != 0;
}
static int perm_can_write(const inode_t* in, int uid){
    if(uid == 0) return 1;
    if(in->uid == (uint16_t)uid) return (in->mode & 0200) != 0;
    return (in->mode & 0002) != 0;
}

// ======= 物理块映射 =======
// 将逻辑块号 bn 映射到物理块号（写路径：必要时分配）
static int map_bn_for_write(inode_t* in, uint32_t bn){
    // 直指针
    if(bn < NDIRECT){
        if(in->direct[bn]==0){
            int b = alloc_block(); if(b < 0) return b;
            // 新分配的数据块清零
            uint8_t zero[BLOCK_SIZE]={0}; dev_write_block(zero,(uint32_t)b);
            in->direct[bn] = (uint32_t)b;
            in->blocks++;
            ts_now(&in->ctime);
        }
        return (int)in->direct[bn];
    }

    // 单级间接
    uint32_t idx = bn - NDIRECT;
    if(idx >= BLOCK_SIZE/4) return FS_ENOSPC; // 超出单级间接范围

    if(in->indirect1 == 0){
        // 分配间接表块
        int b = alloc_block(); if(b < 0) return b;
        uint8_t zero[BLOCK_SIZE]={0};
        dev_write_block(zero,(uint32_t)b);
        in->indirect1 = (uint32_t)b;
        ts_now(&in->ctime);
    }

    uint32_t tbl[BLOCK_SIZE/4];
    if(dev_read_block(tbl, in->indirect1) != FS_OK) return FS_ERR;

    if(tbl[idx] == 0){
        int b = alloc_block(); if(b < 0) return b;
        // 清零新数据块
        uint8_t zero[BLOCK_SIZE]={0}; dev_write_block(zero,(uint32_t)b);
        tbl[idx] = (uint32_t)b;
        if(dev_write_block(tbl, in->indirect1) != FS_OK) return FS_ERR;
        in->blocks++;
        ts_now(&in->ctime);
    }
    return (int)tbl[idx];
}

// 将逻辑块号 bn 映射到物理块号（读路径：不分配）
static int map_bn_for_read(inode_t* in, uint32_t bn){
    if(bn < NDIRECT){
        return in->direct[bn] ? (int)in->direct[bn] : FS_ERR;
    }
    if(in->indirect1 == 0) return FS_ERR;

    uint32_t idx = bn - NDIRECT;
    if(idx >= BLOCK_SIZE/4) return FS_ERR;

    uint32_t tbl[BLOCK_SIZE/4];
    if(dev_read_block(tbl, in->indirect1) != FS_OK) return FS_ERR;
    return tbl[idx] ? (int)tbl[idx] : FS_ERR;
}

// ======= 打开文件 =======
// 支持 "r"（只读）与 "w"（可写；如不存在则创建，不自动截断）
int fs_open(const char* path, const char* mode){
    uint32_t ino;
    int writable = (mode && strchr(mode,'w') != NULL);
    int r = namei(path, &ino);

    if(r != FS_OK){
        // 不存在：仅在可写模式下创建
        if(!writable) return r;

        // 解析父目录与叶子名
        const char* slash = strrchr(path, '/');
        uint32_t dir = (slash ? ((path[0]=='/') ? g_sb.root_ino : g_cwd) : g_cwd);
        char name[NAME_MAX_LEN];

        if(slash){
            // 父路径（可能为空 -> "/"）
            char parent[256];
            size_t n = (size_t)(slash - path);
            if(n >= sizeof(parent)) return FS_ERR;
            memcpy(parent, path, n);
            parent[n] = '\0';
            if(parent[0] == '\0') strcpy(parent, "/");
            if(namei(parent, &dir) != FS_OK) return FS_ERR;

            strncpy(name, slash+1, NAME_MAX_LEN-1);
            name[NAME_MAX_LEN-1]='\0';
        }else{
            strncpy(name, path, NAME_MAX_LEN-1);
            name[NAME_MAX_LEN-1]='\0';
        }

        // 分配 inode 并初始化
        int nino = alloc_inode(); if(nino < 0) return nino;
        inode_t in = (inode_t){0};
        in.mode  = MODE_FILE;        // 默认 0644
        in.links = 1;
        in.uid   = (uint16_t)g_uid;  // 记录所有者
        ts_now(&in.ctime); ts_now(&in.mtime); ts_now(&in.atime);

        if(write_inode((uint32_t)nino, &in) != FS_OK) return FS_ERR;
        if(dir_add(dir, name, FT_REG, (uint32_t)nino) != FS_OK) return FS_ERR;

        ino = (uint32_t)nino;
    }else{
        // 已存在：在打开时先做权限粗校验
        inode_t in;
        if(read_inode(ino, &in) != FS_OK) return FS_ERR;
        if(writable){
            if(!perm_can_write(&in, g_uid)) return FS_EPERM;
        }else{
            if(!perm_can_read(&in, g_uid)) return FS_EPERM;
        }
    }

    // 分配进程内 fd 槽
    for(int fd=0; fd<MAX_OPEN; ++fd){
        if(!g_ofile[fd].used){
            g_ofile[fd].used     = 1;
            g_ofile[fd].ino      = ino;
            g_ofile[fd].offset   = 0;
            g_ofile[fd].writable = writable;
            return fd;
        }
    }
    return FS_ERR;
}

int fs_close(int fd){
    if(fd<0 || fd>=MAX_OPEN || !g_ofile[fd].used) return FS_EBADF;
    g_ofile[fd].used = 0;
    return FS_OK;
}

int fs_seek(int fd, int32_t off){
    if(fd<0 || fd>=MAX_OPEN || !g_ofile[fd].used) return FS_EBADF;
    g_ofile[fd].offset = (off < 0) ? 0u : (uint32_t)off;
    return FS_OK;
}

// ======= 读 =======
int fs_read(int fd, void* buf, uint32_t len){
    if(fd<0 || fd>=MAX_OPEN || !g_ofile[fd].used) return FS_EBADF;

    inode_t in;
    if(read_inode(g_ofile[fd].ino, &in) != FS_OK) return FS_ERR;

    // 权限校验
    if(!perm_can_read(&in, g_uid)) return FS_EPERM;

    uint8_t* out = (uint8_t*)buf;
    uint32_t pos = g_ofile[fd].offset;

    if(pos >= in.size) return 0;                // EOF
    uint32_t remain = in.size - pos;
    if(len > remain) len = remain;

    uint32_t done = 0;
    while(done < len){
        uint32_t bn = pos / BLOCK_SIZE;
        uint32_t boff = pos % BLOCK_SIZE;
        int phys = map_bn_for_read(&in, bn);
        if(phys < 0) break;

        uint8_t blk[BLOCK_SIZE];
        if(dev_read_block(blk, (uint32_t)phys) != FS_OK) return FS_ERR;

        uint32_t can = BLOCK_SIZE - boff;
        if(can > len - done) can = len - done;

        memcpy(out + done, blk + boff, can);
        done += can;
        pos  += can;
    }

    g_ofile[fd].offset = pos;
    ts_now(&in.atime);
    write_inode(g_ofile[fd].ino, &in);
    return (int)done;
}

// ======= 写 =======
int fs_write(int fd, const void* buf, uint32_t len){
    if(fd<0 || fd>=MAX_OPEN || !g_ofile[fd].used) return FS_EBADF;
    if(!g_ofile[fd].writable) return FS_EPERM;

    inode_t in;
    if(read_inode(g_ofile[fd].ino, &in) != FS_OK) return FS_ERR;

    // 权限校验
    if(!perm_can_write(&in, g_uid)) return FS_EPERM;

    const uint8_t* inbuf = (const uint8_t*)buf;
    uint32_t pos = g_ofile[fd].offset;
    uint32_t done = 0;

    while(done < len){
        uint32_t bn   = pos / BLOCK_SIZE;
        uint32_t boff = pos % BLOCK_SIZE;

        int phys = map_bn_for_write(&in, bn);
        if(phys < 0) return phys;

        uint8_t blk[BLOCK_SIZE];
        if(dev_read_block(blk, (uint32_t)phys) != FS_OK) return FS_ERR;

        uint32_t can = BLOCK_SIZE - boff;
        if(can > len - done) can = len - done;

        memcpy(blk + boff, inbuf + done, can);
        if(dev_write_block(blk, (uint32_t)phys) != FS_OK) return FS_ERR;

        done += can;
        pos  += can;
    }

    if(pos > in.size) in.size = pos;
    ts_now(&in.mtime);
    if(write_inode(g_ofile[fd].ino, &in) != FS_OK) return FS_ERR;

    g_ofile[fd].offset = pos;
    return (int)done;
}
