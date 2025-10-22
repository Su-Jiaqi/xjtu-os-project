#include <string.h>
#include "fs.h"

static int bitop(uint8_t* bm, uint32_t idx, int set, int val){
    uint32_t byte = idx>>3, bit=idx&7;
    if(set){
        if(val) bm[byte]|=(1u<<bit); else bm[byte]&=~(1u<<bit);
        return 0;
    }else{
        return (bm[byte]>>bit)&1u;
    }
}

int bmap_test(uint32_t idx, int is_block){
    uint8_t buf[BLOCK_SIZE];
    dev_read_block(buf, is_block? g_sb.block_bitmap_blk : g_sb.inode_bitmap_blk);
    return bitop(buf, idx, 0, 0);
}
int bmap_set(uint32_t idx, int is_block, int val){
    uint8_t buf[BLOCK_SIZE];
    dev_read_block(buf, is_block? g_sb.block_bitmap_blk : g_sb.inode_bitmap_blk);
    bitop(buf, idx, 1, val);
    return dev_write_block(buf, is_block? g_sb.block_bitmap_blk : g_sb.inode_bitmap_blk);
}

int alloc_block(){
    uint8_t bm[BLOCK_SIZE];
    if(dev_read_block(bm, g_sb.block_bitmap_blk)!=FS_OK) return FS_ERR;
    for(uint32_t i=BLK_DATA_START;i<TOTAL_BLOCKS;i++){
        if(!((bm[i>>3]>>(i&7))&1u)){
            bm[i>>3] |= (1u<<(i&7));
            if(dev_write_block(bm, g_sb.block_bitmap_blk)!=FS_OK) return FS_ERR;
            g_sb.free_blocks--; dev_write_block(&g_sb, BLK_SUPER);
            g_gd.free_blocks_count--; dev_write_block(&g_gd, BLK_GDESC);
            return (int)i;
        }
    }
    return FS_ENOSPC;
}
void free_block(uint32_t blk){
    if(blk<BLK_DATA_START || blk>=TOTAL_BLOCKS) return;
    if(bmap_test(blk,1)==0) return;
    bmap_set(blk,1,0);
    g_sb.free_blocks++; dev_write_block(&g_sb, BLK_SUPER);
    g_gd.free_blocks_count++; dev_write_block(&g_gd, BLK_GDESC);
}

int alloc_inode(){
    uint8_t bm[BLOCK_SIZE];
    if(dev_read_block(bm, g_sb.inode_bitmap_blk)!=FS_OK) return FS_ERR;
    for(uint32_t i=1;i<=MAX_INODES;i++){
        if(!((bm[i>>3]>>(i&7))&1u)){
            bm[i>>3] |= (1u<<(i&7));
            if(dev_write_block(bm, g_sb.inode_bitmap_blk)!=FS_OK) return FS_ERR;
            g_sb.free_inodes--; dev_write_block(&g_sb, BLK_SUPER);
            g_gd.free_inodes_count--; dev_write_block(&g_gd, BLK_GDESC);
            return (int)i;
        }
    }
    return FS_ENOSPC;
}
void free_inode(uint32_t ino){
    if(ino==0 || ino>MAX_INODES) return;
    if(bmap_test(ino,0)==0) return;
    bmap_set(ino,0,0);
    g_sb.free_inodes++; dev_write_block(&g_sb, BLK_SUPER);
    g_gd.free_inodes_count++; dev_write_block(&g_gd, BLK_GDESC);
}
