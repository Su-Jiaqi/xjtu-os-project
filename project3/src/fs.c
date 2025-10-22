#include <string.h>
#include "fs.h"

int fs_format(){
    dev_close();
    if(dev_open("disk.img","wb+")!=FS_OK) return FS_ERR;

    // 清盘
    uint8_t zero[BLOCK_SIZE]={0};
    for(uint32_t i=0;i<TOTAL_BLOCKS;i++) dev_write_block(zero,i);

    // 初始化 SB/GD
    memset(&g_sb,0,sizeof(g_sb));
    g_sb.magic=FS_MAGIC; g_sb.block_size=BLOCK_SIZE; g_sb.blocks_count=TOTAL_BLOCKS;
    g_sb.inodes_count=MAX_INODES; g_sb.free_inodes=MAX_INODES; g_sb.free_blocks=TOTAL_BLOCKS;
    g_sb.first_data_block=BLK_DATA_START; g_sb.inode_table_start=BLK_ITBL_START;
    g_sb.block_bitmap_blk=BLK_BMAP; g_sb.inode_bitmap_blk=BLK_IMAP; g_sb.root_ino=1;
    ts_now((uint32_t*)&g_sb.mount_time); ts_now((uint32_t*)&g_sb.write_time);

    memset(&g_gd,0,sizeof(g_gd));
    g_gd.block_bitmap=BLK_BMAP; g_gd.inode_bitmap=BLK_IMAP; g_gd.inode_table=BLK_ITBL_START;
    g_gd.free_blocks_count=TOTAL_BLOCKS; g_gd.free_inodes_count=MAX_INODES;

    dev_write_block(&g_sb, BLK_SUPER);
    dev_write_block(&g_gd, BLK_GDESC);

    // 预留元数据块
    for(uint32_t i=0;i<BLK_DATA_START;i++) bmap_set(i,1,1);
    g_sb.free_blocks -= BLK_DATA_START; g_gd.free_blocks_count -= BLK_DATA_START;
    dev_write_block(&g_sb, BLK_SUPER); dev_write_block(&g_gd, BLK_GDESC);

    // 根 inode
    bmap_set(1,0,1); g_sb.free_inodes--; g_gd.free_inodes_count--;
    dev_write_block(&g_sb, BLK_SUPER); dev_write_block(&g_gd, BLK_GDESC);

    inode_t root={0}; root.mode=MODE_DIR; root.links=2; ts_now(&root.ctime); ts_now(&root.mtime); ts_now(&root.atime);
    write_inode(1,&root);
    // '.' '..'
    dir_add(1, ".", FT_DIR, 1);
    dir_add(1, "..", FT_DIR, 1);

    dev_close();
    return FS_OK;
}
int fs_mount(const char* img){
    if(dev_open(img,"rb+")!=FS_OK) return FS_ERR;
    if(dev_read_block(&g_sb, BLK_SUPER)!=FS_OK) return FS_ERR;
    if(g_sb.magic!=FS_MAGIC || g_sb.block_size!=BLOCK_SIZE) return FS_ERR;
    if(dev_read_block(&g_gd, BLK_GDESC)!=FS_OK) return FS_ERR;
    g_cwd=g_sb.root_ino; memset(g_ofile,0,sizeof(g_ofile));
    return FS_OK;
}
