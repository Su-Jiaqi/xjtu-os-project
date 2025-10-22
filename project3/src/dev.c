#include <string.h>
#include "fs.h"

superblock_t g_sb;
group_desc_t g_gd;
FILE* g_dev = NULL;
ofile_t g_ofile[MAX_OPEN] = {0};
uint32_t g_cwd = 1;

int  g_uid = 0;                 // 初始 root
char g_user[MAX_USER_LEN] = "root";

int dev_open(const char* path, const char* mode){
    if(g_dev) return FS_OK;
    g_dev = fopen(path, mode);
    return g_dev ? FS_OK : FS_ERR;
}
int dev_close(){
    if(!g_dev) return FS_OK;
    int r=fclose(g_dev); g_dev=NULL; return r==0?FS_OK:FS_ERR;
}
int dev_read_block(void* buf, uint32_t blk_no){
    if(!g_dev || blk_no>=TOTAL_BLOCKS) return FS_ERR;
    if(fseek(g_dev, (long)blk_no*BLOCK_SIZE, SEEK_SET)!=0) return FS_ERR;
    return fread(buf,1,BLOCK_SIZE,g_dev)==BLOCK_SIZE?FS_OK:FS_ERR;
}
int dev_write_block(const void* buf, uint32_t blk_no){
    if(!g_dev || blk_no>=TOTAL_BLOCKS) return FS_ERR;
    if(fseek(g_dev, (long)blk_no*BLOCK_SIZE, SEEK_SET)!=0) return FS_ERR;
    if(fwrite(buf,1,BLOCK_SIZE,g_dev)!=BLOCK_SIZE) return FS_ERR;
    fflush(g_dev);
    return FS_OK;
}
