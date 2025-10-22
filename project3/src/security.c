// src/security.c — 极简用户/口令 & 会话持久化
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "fs.h"

// ===== 辅助：一次性读取整个文件到内存 =====
static int read_whole_file(const char* path, char** out_s, uint32_t* out_len){
    *out_s = NULL; *out_len = 0;
    int fd = fs_open(path, "r");
    if(fd < 0) return fd;

    char* buf = NULL; uint32_t cap = 0, total = 0;
    for(;;){
        if(cap - total < 4096){
            uint32_t ncap = cap ? cap * 2u : 4096u;
            char* nbuf = (char*)realloc(buf, ncap);
            if(!nbuf){ free(buf); fs_close(fd); return FS_ERR; }
            buf = nbuf; cap = ncap;
        }
        int r = fs_read(fd, buf + total, 4096);
        if(r < 0){ free(buf); fs_close(fd); return r; }
        if(r == 0) break;
        total += (uint32_t)r;
    }
    fs_close(fd);
    *out_s = buf; *out_len = total;
    return FS_OK;
}

// ===== 辅助：把一行文本追加到文件尾（不截断）=====
static int append_line(const char* path, const char* line){
    int fd = fs_open(path, "w");     // 我们的 "w" = 可写（不截断）
    if(fd < 0) return fd;

    // seek 到末尾（用 inode 大小）
    inode_t in;
    if(read_inode(g_ofile[fd].ino, &in) != FS_OK){ fs_close(fd); return FS_ERR; }
    fs_seek(fd, (int32_t)in.size);

    int n = fs_write(fd, line, (uint32_t)strlen(line));
    fs_close(fd);
    return (n < 0) ? n : FS_OK;
}

// ===== 解析一行 "name:pass:uid" =====
static int parse_line(const char* line, char* name, char* pass, int* uid){
    const char* p = strchr(line, ':'); if(!p) return -1;
    size_t n1 = (size_t)(p - line);

    const char* q = strchr(p + 1, ':'); if(!q) return -1;
    size_t n2 = (size_t)(q - (p + 1));

    const char* e = strchr(q + 1, '\n'); if(!e) e = line + strlen(line);
    size_t n3 = (size_t)(e - (q + 1));

    if(n1 >= MAX_USER_LEN || n2 >= MAX_USER_LEN || n3 >= 16) return -1;

    memcpy(name, line, n1); name[n1] = '\0';
    memcpy(pass, p + 1, n2); pass[n2] = '\0';

    char uidbuf[32];
    if(n3 >= sizeof(uidbuf)) return -1;
    memcpy(uidbuf, q + 1, n3); uidbuf[n3] = '\0';
    *uid = atoi(uidbuf);
    return 0;
}

// ===== 会话持久化：/.session =====
int session_save(int uid, const char* user){
    int fd = fs_open("/.session", "w");
    if(fd < 0) return fd;

    // 截断到 0
    inode_t in;
    if(read_inode(g_ofile[fd].ino, &in) == FS_OK){
        in.size = 0;
        if(write_inode(g_ofile[fd].ino, &in) != FS_OK){ fs_close(fd); return FS_ERR; }
    }
    fs_seek(fd, 0);

    char line[128];
    int n = snprintf(line, sizeof(line), "uid:%d\nuser:%s\n", uid, user ? user : "");
    int w = fs_write(fd, line, (uint32_t)n);
    fs_close(fd);
    return (w < 0) ? w : FS_OK;
}

int session_load(){
    uint32_t ino;
    if(namei("/.session", &ino) != FS_OK) return FS_ERR;  // 没有会话文件就返回错误

    int fd = fs_open("/.session", "r");
    if(fd < 0) return fd;

    char buf[128] = {0};
    int r = fs_read(fd, buf, sizeof(buf) - 1);
    fs_close(fd);
    if(r <= 0) return FS_ERR;

    int uid = 0; char user[MAX_USER_LEN] = {0};

    char* pu = strstr(buf, "uid:");
    if(pu) uid = atoi(pu + 4);

    char* pn = strstr(buf, "user:");
    if(pn){
        pn += 5;
        size_t k = 0;
        while(*pn && *pn != '\n' && k < MAX_USER_LEN - 1){
            user[k++] = *pn++;
        }
        user[k] = '\0';
    }
    if(user[0] == '\0') return FS_ERR;

    g_uid = uid;
    strncpy(g_user, user, MAX_USER_LEN - 1);
    g_user[MAX_USER_LEN - 1] = '\0';
    return FS_OK;
}

// ===== 引导：确保有 /.users，写入 root:root:0 =====
int users_bootstrap(){
    uint32_t uino;
    if(namei("/.users", &uino) == FS_OK) return FS_OK;

    int fd = fs_open("/.users", "w");
    if(fd < 0) return fd;

    const char* init = "root:root:0\n";
    int n = fs_write(fd, init, (uint32_t)strlen(init));
    fs_close(fd);
    return (n < 0) ? n : FS_OK;
}

// ===== 登录：匹配 name/pass，设置 g_uid/g_user，并保存会话 =====
int users_login(const char* name, const char* pass){
    char* s = NULL; uint32_t n = 0;
    if(read_whole_file("/.users", &s, &n) != FS_OK) return FS_ERR;

    int ok = 0; int uid = -1;
    for(char* p = s; p && *p; ){
        char* nl = strchr(p, '\n');
        size_t ln = nl ? (size_t)(nl - p) : strlen(p);

        char line[256];
        if(ln >= sizeof(line)) ln = sizeof(line) - 1;
        memcpy(line, p, ln); line[ln] = '\0';

        char uname[MAX_USER_LEN], upass[MAX_USER_LEN]; int u = -1;
        if(parse_line(line, uname, upass, &u) == 0){
            if(strcmp(uname, name) == 0 && strcmp(upass, pass) == 0){
                uid = u; ok = 1; 
                break;
            }
        }
        if(!nl) break;
        p = nl + 1;
    }
    free(s);

    if(!ok) return FS_ERR;

    g_uid = uid;
    strncpy(g_user, name, MAX_USER_LEN - 1);
    g_user[MAX_USER_LEN - 1] = '\0';

    session_save(g_uid, g_user);   // 持久化当前会话
    return FS_OK;
}

// ===== 新增用户：root 才能添加，自动分配 uid = max+1 =====
int users_add(const char* name, const char* pass){
    if(g_uid != 0) return FS_EPERM;

    char* s = NULL; uint32_t n = 0;
    if(read_whole_file("/.users", &s, &n) != FS_OK) return FS_ERR;

    int max_uid = 0;
    for(char* p = s; p && *p; ){
        char* nl = strchr(p, '\n');
        size_t ln = nl ? (size_t)(nl - p) : strlen(p);

        char line[256];
        if(ln >= sizeof(line)) ln = sizeof(line) - 1;
        memcpy(line, p, ln); line[ln] = '\0';

        char uname[MAX_USER_LEN], upass[MAX_USER_LEN]; int u = -1;
        if(parse_line(line, uname, upass, &u) == 0){
            if(strcmp(uname, name) == 0){
                free(s);
                return FS_EEXIST;
            }
            if(u > max_uid) max_uid = u;
        }
        if(!nl) break;
        p = nl + 1;
    }
    free(s);

    int new_uid = max_uid + 1;
    char line[256];
    snprintf(line, sizeof(line), "%s:%s:%d\n", name, pass, new_uid);
    return append_line("/.users", line);
}

// ===== 修改口令：root 或本人可改，重写整个 /.users 文件 =====
int users_change_password(const char* name, const char* pass){
    if(!(g_uid == 0 || strcmp(g_user, name) == 0)) return FS_EPERM;

    char* s = NULL; uint32_t n = 0;
    if(read_whole_file("/.users", &s, &n) != FS_OK) return FS_ERR;

    int fd = fs_open("/.users", "w");
    if(fd < 0){ free(s); return fd; }

    // 截断文件
    inode_t in;
    if(read_inode(g_ofile[fd].ino, &in) == FS_OK){
        in.size = 0;
        if(write_inode(g_ofile[fd].ino, &in) != FS_OK){ fs_close(fd); free(s); return FS_ERR; }
    }
    fs_seek(fd, 0);

    // 逐行重写（目标用户替换为新密码）
    for(char* p = s; p && *p; ){
        char* nl = strchr(p, '\n');
        size_t ln = nl ? (size_t)(nl - p) : strlen(p);

        char line[256];
        if(ln >= sizeof(line)) ln = sizeof(line) - 1;
        memcpy(line, p, ln); line[ln] = '\0';

        char uname[MAX_USER_LEN], upass[MAX_USER_LEN]; int u = -1;
        if(parse_line(line, uname, upass, &u) == 0){
            char out[256];
            if(strcmp(uname, name) == 0){
                snprintf(out, sizeof(out), "%s:%s:%d\n", uname, pass, u);
                fs_write(fd, out, (uint32_t)strlen(out));
            }else{
                snprintf(out, sizeof(out), "%s:%s:%d\n", uname, upass, u);
                fs_write(fd, out, (uint32_t)strlen(out));
            }
        }
        if(!nl) break;
        p = nl + 1;
    }
    free(s);
    fs_close(fd);
    return FS_OK;
}
