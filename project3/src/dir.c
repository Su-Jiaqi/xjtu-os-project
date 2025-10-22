#include <string.h>
#include "fs.h"

// 目录：仅用直指针，顺序存 64B dirent
static int read_dir_block(inode_t* in, uint32_t bn, uint8_t* buf){
    if(bn>=NDIRECT) return FS_ERR;
    if(in->direct[bn]==0) return FS_ERR;
    return dev_read_block(buf, in->direct[bn]);
}
static int ensure_dir_block(inode_t* in, uint32_t bn, uint8_t* blkbuf){
    if(bn>=NDIRECT) return FS_ENOSPC;
    if(in->direct[bn]==0){
        int b=alloc_block(); if(b<0) return b;
        memset(blkbuf,0,BLOCK_SIZE); dev_write_block(blkbuf,(uint32_t)b);
        in->direct[bn]=(uint32_t)b; in->blocks++; ts_now(&in->ctime);
    }else{
        dev_read_block(blkbuf, in->direct[bn]);
    }
    return FS_OK;
}

int dir_lookup(uint32_t dir_ino, const char* name, uint32_t* out_ino){
    inode_t din; if(read_inode(dir_ino,&din)!=FS_OK) return FS_ERR;
    if((din.mode & 0170000)!=0040000) return FS_ENOTDIR;
    uint32_t n=din.size/sizeof(dirent_t);
    uint8_t blk[BLOCK_SIZE]; dirent_t de;
    for(uint32_t i=0;i<n;i++){
        uint32_t bn=(i*sizeof(dirent_t))/BLOCK_SIZE, off=(i*sizeof(dirent_t))%BLOCK_SIZE;
        if(read_dir_block(&din,bn,blk)!=FS_OK) continue;
        memcpy(&de, blk+off, sizeof(de));
        if(de.ino!=0 && strncmp(de.name,name,NAME_MAX_LEN)==0){
            *out_ino=de.ino; return FS_OK;
        }
    }
    return FS_ENOENT;
}
int dir_add(uint32_t dir_ino, const char* name, uint8_t ftype, uint32_t child_ino){
    inode_t din; if(read_inode(dir_ino,&din)!=FS_OK) return FS_ERR;
    if((din.mode & 0170000)!=0040000) return FS_ENOTDIR;
    uint32_t idx=din.size/sizeof(dirent_t);
    uint32_t bn=(idx*sizeof(dirent_t))/BLOCK_SIZE, off=(idx*sizeof(dirent_t))%BLOCK_SIZE;
    uint8_t blk[BLOCK_SIZE]; if(ensure_dir_block(&din,bn,blk)!=FS_OK) return FS_ENOSPC;

    dirent_t de={0}; de.ino=child_ino; de.reclen=sizeof(dirent_t); de.file_type=ftype;
    strncpy(de.name,name,NAME_MAX_LEN-1);
    memcpy(blk+off, &de, sizeof(de));
    if(dev_write_block(blk, din.direct[bn])!=FS_OK) return FS_ERR;
    din.size += sizeof(dirent_t); ts_now(&din.mtime);
    return write_inode(dir_ino,&din);
}
int dir_remove(uint32_t dir_ino, const char* name){
    inode_t din; if(read_inode(dir_ino,&din)!=FS_OK) return FS_ERR;
    uint32_t n=din.size/sizeof(dirent_t); uint8_t blk[BLOCK_SIZE]; dirent_t de;
    for(uint32_t i=0;i<n;i++){
        uint32_t bn=(i*sizeof(dirent_t))/BLOCK_SIZE, off=(i*sizeof(dirent_t))%BLOCK_SIZE;
        if(read_dir_block(&din,bn,blk)!=FS_OK) continue;
        memcpy(&de, blk+off, sizeof(de));
        if(de.ino!=0 && strncmp(de.name,name,NAME_MAX_LEN)==0){
            memset(blk+off,0,sizeof(de));
            if(dev_write_block(blk, din.direct[bn])!=FS_OK) return FS_ERR;
            ts_now(&din.mtime); write_inode(dir_ino,&din); return FS_OK;
        }
    }
    return FS_ENOENT;
}

// 极简路径解析：支持绝对/相对，忽略 . ..
int namei(const char* path, uint32_t* out_ino){
    if(!path||!*path) return FS_ERR;
    uint32_t cur = (path[0]=='/')? g_sb.root_ino : g_cwd;
    const char* p=path; if(*p=='/') p++;
    char seg[NAME_MAX_LEN];
    while(*p){
        int k=0; while(*p && *p!='/'){ if(k<NAME_MAX_LEN-1) seg[k++]=*p; p++; }
        seg[k]='\0';
        if(k==0){ if(*p=='/') {p++; continue;} else break; }
        uint32_t nxt; int r=dir_lookup(cur, seg, &nxt);
        if(r!=FS_OK) return r;
        cur=nxt;
        if(*p=='/') p++;
    }
    *out_ino=cur; return FS_OK;
}
