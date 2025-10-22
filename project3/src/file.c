#include <string.h>
#include "fs.h"

static int map_bn_for_write(inode_t* in, uint32_t bn){
    if(bn < NDIRECT){
        if(in->direct[bn]==0){
            int b=alloc_block(); if(b<0) return b;
            in->direct[bn]=(uint32_t)b; in->blocks++; ts_now(&in->ctime);
        }
        return (int)in->direct[bn];
    }
    // 单级间接
    if(in->indirect1==0){
        int b=alloc_block(); if(b<0) return b;
        uint8_t zero[BLOCK_SIZE]={0}; dev_write_block(zero,(uint32_t)b);
        in->indirect1=(uint32_t)b; ts_now(&in->ctime);
    }
    uint32_t idx = bn - NDIRECT; if(idx >= BLOCK_SIZE/4) return FS_ENOSPC;
    uint32_t tbl[BLOCK_SIZE/4]; dev_read_block(tbl, in->indirect1);
    if(tbl[idx]==0){
        int b=alloc_block(); if(b<0) return b;
        tbl[idx]=(uint32_t)b; in->blocks++; dev_write_block(tbl, in->indirect1);
    }
    return (int)tbl[idx];
}
static int map_bn_for_read(inode_t* in, uint32_t bn){
    if(bn < NDIRECT){ return in->direct[bn]? (int)in->direct[bn] : FS_ERR; }
    if(in->indirect1==0) return FS_ERR;
    uint32_t idx = bn - NDIRECT; if(idx >= BLOCK_SIZE/4) return FS_ERR;
    uint32_t tbl[BLOCK_SIZE/4]; dev_read_block(tbl, in->indirect1);
    return tbl[idx]? (int)tbl[idx] : FS_ERR;
}

int fs_open(const char* path, const char* mode){
    uint32_t ino; int r=namei(path,&ino);
    int writable = (mode && strchr(mode, 'w') != NULL);
    if(r!=FS_OK){
        if(!writable) return r;
        // 创建：解析父目录
        const char* slash=strrchr(path,'/');
        uint32_t dir = (slash? ((path[0]=='/')? g_sb.root_ino : g_cwd) : g_cwd);
        char name[NAME_MAX_LEN];
        if(slash){
            char parent[256]; size_t n=(size_t)(slash - path);
            if(n>=sizeof(parent)) return FS_ERR;
            memcpy(parent,path,n); parent[n]='\0'; if(parent[0]=='\0') strcpy(parent,"/");
            if(namei(parent,&dir)!=FS_OK) return FS_ERR;
            strncpy(name, slash+1, NAME_MAX_LEN-1); name[NAME_MAX_LEN-1]='\0';
        }else{ strncpy(name, path, NAME_MAX_LEN-1); name[NAME_MAX_LEN-1]='\0'; }
        int nino=alloc_inode(); if(nino<0) return nino;
        inode_t in={0}; in.mode=MODE_FILE; in.links=1; ts_now(&in.ctime); ts_now(&in.mtime); ts_now(&in.atime);
        write_inode((uint32_t)nino,&in);
        dir_add(dir, name, FT_REG, (uint32_t)nino);
        ino=(uint32_t)nino;
    }
    for(int fd=0; fd<MAX_OPEN; fd++)
        if(!g_ofile[fd].used){ g_ofile[fd].used=1; g_ofile[fd].ino=ino; g_ofile[fd].offset=0; g_ofile[fd].writable=writable; return fd; }
    return FS_ERR;
}
int fs_close(int fd){ if(fd<0||fd>=MAX_OPEN||!g_ofile[fd].used) return FS_EBADF; g_ofile[fd].used=0; return FS_OK; }
int fs_seek(int fd, int32_t off){ if(fd<0||fd>=MAX_OPEN||!g_ofile[fd].used) return FS_EBADF; g_ofile[fd].offset=(off<0?0:(uint32_t)off); return FS_OK; }

int fs_read(int fd, void* buf, uint32_t len){
    if(fd<0||fd>=MAX_OPEN||!g_ofile[fd].used) return FS_EBADF;
    inode_t in; if(read_inode(g_ofile[fd].ino,&in)!=FS_OK) return FS_ERR;
    uint8_t* out=(uint8_t*)buf; uint32_t pos=g_ofile[fd].offset; if(pos>=in.size) return 0;
    uint32_t remain=in.size-pos; if(len>remain) len=remain; uint32_t done=0;
    while(done<len){
        uint32_t bn=pos/BLOCK_SIZE, boff=pos%BLOCK_SIZE;
        int phys=map_bn_for_read(&in,bn); if(phys<0) break;
        uint8_t blk[BLOCK_SIZE]; dev_read_block(blk,(uint32_t)phys);
        uint32_t can=BLOCK_SIZE-boff; if(can>len-done) can=len-done;
        memcpy(out+done, blk+boff, can); done+=can; pos+=can;
    }
    g_ofile[fd].offset=pos; ts_now(&in.atime); write_inode(g_ofile[fd].ino,&in);
    return (int)done;
}
int fs_write(int fd, const void* buf, uint32_t len){
    if(fd<0||fd>=MAX_OPEN||!g_ofile[fd].used) return FS_EBADF;
    if(!g_ofile[fd].writable) return FS_EPERM;
    inode_t in; if(read_inode(g_ofile[fd].ino,&in)!=FS_OK) return FS_ERR;
    const uint8_t* inbuf=(const uint8_t*)buf; uint32_t pos=g_ofile[fd].offset; uint32_t done=0;
    while(done<len){
        uint32_t bn=pos/BLOCK_SIZE, boff=pos%BLOCK_SIZE;
        int phys=map_bn_for_write(&in,bn); if(phys<0) return phys;
        uint8_t blk[BLOCK_SIZE]; dev_read_block(blk,(uint32_t)phys);
        uint32_t can=BLOCK_SIZE-boff; if(can>len-done) can=len-done;
        memcpy(blk+boff, inbuf+done, can); dev_write_block(blk,(uint32_t)phys);
        done+=can; pos+=can;
    }
    if (pos > in.size)
        in.size = pos;
    ts_now(&in.mtime);
    write_inode(g_ofile[fd].ino, &in);

    g_ofile[fd].offset=pos; return (int)done;
}
