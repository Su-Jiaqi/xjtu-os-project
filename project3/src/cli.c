#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "fs.h"

static void cmd_ls(const char* path){
    uint32_t ino;
    if(path && *path){ if(namei(path,&ino)!=FS_OK){ puts("ls: no such file/dir"); return; } }
    else ino = g_cwd;

    inode_t din;
    if(read_inode(ino,&din)!=FS_OK){ puts("ls: read inode fail"); return; }
    if((din.mode & 0170000) != 0040000){ puts("ls: not a directory"); return; }

    printf("mode       uid  type name                                               size      ctime                mtime                atime\n");
    uint32_t n = din.size/sizeof(dirent_t);
    uint8_t blk[BLOCK_SIZE];
    dirent_t de;

    for(uint32_t i=0;i<n;i++){
        uint32_t bn=(i*sizeof(dirent_t))/BLOCK_SIZE, off=(i*sizeof(dirent_t))%BLOCK_SIZE;
        if(din.direct[bn]==0) continue;
        if(dev_read_block(blk,din.direct[bn])!=FS_OK) continue;
        memcpy(&de, blk+off, sizeof(de));
        if(de.ino==0) continue;

        inode_t child; if(read_inode(de.ino,&child)!=FS_OK) continue;
        char ms[11], ct[20], mt[20], at[20];
        mode_to_str(child.mode, ms);
        human_time(child.ctime, ct, sizeof ct);
        human_time(child.mtime, mt, sizeof mt);
        human_time(child.atime, at, sizeof at);

        printf("%-10s %-3u  %-4s %-50s %-8u %-19s %-19s %-19s\n",
               ms, (unsigned)child.uid, (de.file_type==FT_DIR?"dir":"file"),
               de.name, child.size, ct, mt, at);
    }
}

static void cmd_format(){ puts(fs_format()==FS_OK? "[OK] formatted":"[ERR] format fail"); }
static void cmd_mount(){ puts(fs_mount("disk.img")==FS_OK? "[OK] mounted":"[ERR] mount fail"); }

static void cmd_mkdir(const char* path){
    uint32_t parent; const char* slash=strrchr(path,'/'); char name[NAME_MAX_LEN];
    if(!slash){ parent=g_cwd; strncpy(name,path,NAME_MAX_LEN-1); name[NAME_MAX_LEN-1]='\0'; }
    else{
        char pbuf[256]; size_t n=(size_t)(slash-path); if(n>=sizeof(pbuf)){ puts("mkdir: path too long"); return; }
        memcpy(pbuf,path,n); pbuf[n]='\0'; if(!*pbuf) strcpy(pbuf,"/");
        if(namei(pbuf,&parent)!=FS_OK){ puts("mkdir: parent missing"); return; }
        strncpy(name,slash+1,NAME_MAX_LEN-1); name[NAME_MAX_LEN-1]='\0';
    }
    int ino=alloc_inode(); if(ino<0){ puts("mkdir: no inode"); return; }
    inode_t in={0}; in.mode=MODE_DIR; in.links=2; in.uid=(uint16_t)g_uid;
    ts_now(&in.ctime); ts_now(&in.mtime); ts_now(&in.atime);
    write_inode((uint32_t)ino,&in);
    dir_add((uint32_t)ino, ".", FT_DIR, (uint32_t)ino);
    dir_add((uint32_t)ino, "..", FT_DIR, parent);
    puts(dir_add(parent,name,FT_DIR,(uint32_t)ino)==FS_OK? "[OK]" : "mkdir: dir_add fail");
}

static void cmd_create(const char* path){
    int fd=fs_open(path,"w"); if(fd>=0){ fs_close(fd); puts("[OK]"); } else puts("[ERR]");
}

static void cmd_open(const char* path, const char* m){
    int fd=fs_open(path,m); if(fd<0) puts("[ERR]"); else printf("fd=%d\n",fd);
}
static void cmd_write_fd(int fd, const char* s){ int n=fs_write(fd,s,(uint32_t)strlen(s)); printf("wrote=%d\n", n); }
static void cmd_read_fd (int fd, int n){ char* b=(char*)malloc((size_t)n+1); int r=fs_read(fd,b,(uint32_t)n); b[(r<0)?0:r]='\0'; printf("read=%d: %s\n", r, b); free(b); }
static void cmd_close(int fd){ puts(fs_close(fd)==FS_OK? "close=OK":"close=ERR"); }
static void cmd_cd(const char* path){ uint32_t ino; puts(namei(path,&ino)==FS_OK? (g_cwd=ino,"[OK]") : "[ERR]"); }
static void cmd_seek(int fd, int off){ puts(fs_seek(fd,off)==FS_OK? "seek=OK":"seek=ERR"); }

// 一次性命令（便于测试）
static void cmd_writef(const char* path, const char* s){ int fd=fs_open(path,"w"); if(fd<0){ puts("writef: open fail"); return; } int n=fs_write(fd,s,(uint32_t)strlen(s)); fs_close(fd); printf("wrote=%d\n", n); }

static void cmd_readf(const char* path, int n){
    int fd = fs_open(path,"r");
    if(fd < 0){ puts("readf: open fail"); return; }
    char* b = (char*)malloc((size_t)n+1);
    int r = fs_read(fd, b, (uint32_t)n);
    if(r < 0) { printf("read=%d\n", r); fs_close(fd); free(b); return; }
    b[r] = '\0';
    printf("read=%d: %s\n", r, b);
    free(b);
    fs_close(fd);
}

static void cmd_writefile(const char* fs_path, const char* host_path){
    FILE* f = fopen(host_path,"rb");
    if(!f){ printf("writefile: cannot open %s\n", host_path); return; }
    int fd = fs_open(fs_path,"w");
    if(fd < 0){ puts("writefile: open fail"); fclose(f); return; }

    char buf[BLOCK_SIZE];
    int total = 0;
    for(;;){
        size_t n = fread(buf,1,sizeof(buf),f);
        if(n==0) break;
        int w = fs_write(fd, buf, (uint32_t)n);
        if(w<0){ printf("writefile: fs_write=%d\n", w); break; }
        total += w;
        if(n < sizeof(buf)) break;
    }
    fs_close(fd);
    fclose(f);
    printf("wrotefile=%d bytes\n", total);
}

// delete：文件/空目录
static void cmd_delete(const char* path){
    uint32_t ino; if(namei(path,&ino)!=FS_OK){ puts("delete: noent"); return; }
    inode_t in; read_inode(ino,&in);
    if((in.mode & 0170000)==0040000){ // dir
        if(in.size>2*sizeof(dirent_t)){ puts("delete: dir not empty"); return; }
    }
    inode_truncate(ino); free_inode(ino);
    // 从父目录移除
    const char* s=strrchr(path,'/'); const char* nm=s? s+1 : path; uint32_t p;
    if(s){ char parent[256]; size_t n=(size_t)(s-path); memcpy(parent,path,n); parent[n]='\0'; if(!*parent) strcpy(parent,"/"); namei(parent,&p); }
    else p=g_cwd;
    dir_remove(p,nm); puts("[OK]");
}

// chmod：读写保护
static void cmd_chmod(const char* oct, const char* path){
    uint32_t ino; if(namei(path,&ino)!=FS_OK){ puts("chmod: noent"); return; }
    inode_t in; read_inode(ino,&in);
    if(g_uid!=0 && in.uid!=g_uid){ puts("chmod: EPERM"); return; }
    unsigned m=0; sscanf(oct, "%o", &m);
    in.mode = (uint16_t)((in.mode & 0170000) | (m & 0777));
    ts_now(&in.ctime); write_inode(ino,&in); puts("[OK]");
}

// 账号
static void cmd_login(const char* u, const char* p){ puts(users_login(u,p)==FS_OK? "[OK]":"[ERR] login"); }
static void cmd_password(const char* name, const char* pass){
    puts(users_change_password(name, pass)==FS_OK ? "[OK]" : "[ERR] password");
}

int main(int argc, char** argv){
    if(argc<2){
        puts("Usage:\n"
             "  mini_ext2 format | mount\n"
             "  mini_ext2 login <user> <pass> | password <old> <new>\n"
             "  mini_ext2 ls [path]\n"
             "  mini_ext2 mkdir <path> | create <path> | delete <path>\n"
             "  mini_ext2 open <path> [r|w] | write <fd> <str> | read <fd> <n> | seek <fd> <off> | close <fd>\n"
             "  mini_ext2 writef <path> <str> | readf <path> <n> | writefile <fs_path> <host_path>\n"
             "  mini_ext2 chmod <oct> <path> | cd <path>");
        return 0;
    }

    if(strcmp(argv[1],"format")==0){ cmd_format(); return 0; }
    if(strcmp(argv[1],"mount")==0){ cmd_mount();  return 0; }
    if(fs_mount("disk.img")!=FS_OK){ puts("[ERR] auto-mount disk.img fail (run format first)"); return 1; }

    if(strcmp(argv[1],"login")==0 && argc>=4)      cmd_login(argv[2],argv[3]);
    else if(strcmp(argv[1],"password")==0 && argc>=4) cmd_password(argv[2],argv[3]);
    else if(strcmp(argv[1],"ls")==0)               cmd_ls(argc>=3?argv[2]:NULL);
    else if(strcmp(argv[1],"mkdir")==0 && argc>=3) cmd_mkdir(argv[2]);
    else if(strcmp(argv[1],"create")==0 && argc>=3)cmd_create(argv[2]);
    else if(strcmp(argv[1],"open")==0 && argc>=3)  cmd_open(argv[2], argc>=4?argv[3]:"r");
    else if(strcmp(argv[1],"write")==0 && argc>=4) cmd_write_fd(atoi(argv[2]), argv[3]);
    else if(strcmp(argv[1],"read")==0 && argc>=4)  cmd_read_fd(atoi(argv[2]), atoi(argv[3]));
    else if(strcmp(argv[1],"close")==0 && argc>=3) cmd_close(atoi(argv[2]));
    else if(strcmp(argv[1],"cd")==0 && argc>=3)    cmd_cd(argv[2]);
    else if(strcmp(argv[1],"seek")==0 && argc>=4)  cmd_seek(atoi(argv[2]), atoi(argv[3]));
    else if(strcmp(argv[1],"writef")==0 && argc>=4)cmd_writef(argv[2], argv[3]);
    else if(strcmp(argv[1],"readf")==0 && argc>=4) cmd_readf(argv[2], atoi(argv[3]));
    else if(strcmp(argv[1],"writefile")==0 && argc>=4) cmd_writefile(argv[2], argv[3]);
    else if(strcmp(argv[1],"chmod")==0 && argc>=4) cmd_chmod(argv[2], argv[3]);
    else if(strcmp(argv[1],"delete")==0 && argc>=3) cmd_delete(argv[2]);
    else if(strcmp(argv[1],"login")==0 && argc>=4){
        int r = users_login(argv[2], argv[3]);
        puts(r==FS_OK ? "[OK]" : "[ERR] login");
    }
    else if(strcmp(argv[1],"useradd")==0 && argc>=4){
        int r = users_add(argv[2], argv[3]);
        puts(r==FS_OK ? "[OK]" : "[ERR] useradd");
    }
    else if(strcmp(argv[1],"password")==0 && argc>=4){
        int r = users_change_password(argv[2], argv[3]);
        puts(r==FS_OK ? "[OK]" : "[ERR] password");
    } 
    // 调试查看当前身份
    else if(strcmp(argv[1],"whoami")==0){
        printf("%s (uid=%d)\n", g_user, g_uid);
    }
    else if(strcmp(argv[1],"chmod")==0 && argc>=4){ /* 你现有的 chmod 实现保持 */ }
    else if(strcmp(argv[1],"writef")==0 && argc>=4){ cmd_writef(argv[2], argv[3]); }
    else if(strcmp(argv[1],"readf")==0  && argc>=4){ cmd_readf(argv[2], atoi(argv[3])); }
    else if(strcmp(argv[1],"writefile")==0 && argc>=4){ cmd_writefile(argv[2], argv[3]); }
    else puts("[ERR] unknown or bad args");

    dev_close();
    return 0;
}
