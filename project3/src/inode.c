#include <string.h>
#include "fs.h"

static int inode_pos(uint32_t ino, uint32_t* blk, uint32_t* off){
    if(ino==0 || ino>MAX_INODES) return FS_ERR;
    uint32_t idx=(ino-1)*INODE_SIZE;
    *blk = g_sb.inode_table_start + (idx / BLOCK_SIZE);
    *off = idx % BLOCK_SIZE;
    return FS_OK;
}
int read_inode(uint32_t ino, inode_t* out){
    uint32_t blk,off; if(inode_pos(ino,&blk,&off)!=FS_OK) return FS_ERR;
    uint8_t buf[BLOCK_SIZE]; if(dev_read_block(buf,blk)!=FS_OK) return FS_ERR;
    memcpy(out, buf+off, sizeof(inode_t));
    return FS_OK;
}
int write_inode(uint32_t ino, const inode_t* in){
    uint32_t blk,off; if(inode_pos(ino,&blk,&off)!=FS_OK) return FS_ERR;
    uint8_t buf[BLOCK_SIZE]; if(dev_read_block(buf,blk)!=FS_OK) return FS_ERR;
    memcpy(buf+off, in, sizeof(inode_t));
    return dev_write_block(buf, blk);
}
int inode_truncate(uint32_t ino){
    inode_t in; if(read_inode(ino,&in)!=FS_OK) return FS_ERR;
    for(int i=0;i<NDIRECT;i++) if(in.direct[i]){ free_block(in.direct[i]); in.direct[i]=0; }
    if(in.indirect1){
        uint32_t tbl[BLOCK_SIZE/4];
        if(dev_read_block(tbl, in.indirect1)==FS_OK){
            for(size_t i=0;i<BLOCK_SIZE/4;i++) if(tbl[i]) free_block(tbl[i]);
        }
        free_block(in.indirect1); in.indirect1=0;
    }
    in.size=0; in.blocks=0; ts_now(&in.mtime); ts_now(&in.ctime);
    return write_inode(ino,&in);
}
