// memsim_minslice.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROCESS_NAME_LEN 32
#define DEFAULT_MEM_SIZE 1024
#define DEFAULT_MEM_START 0

// 算法
#define MA_FF 1
#define MA_BF 2
#define MA_WF 3

typedef struct free_block_type {
    int size;
    int start_addr;
    struct free_block_type *next;
} free_block_type;

typedef struct allocated_block {
    int pid, size, start_addr;
    char process_name[PROCESS_NAME_LEN];
    struct allocated_block *next;
} allocated_block;

// 全局
static free_block_type *free_block = NULL;
static allocated_block *allocated_block_head = NULL;
static int mem_size = DEFAULT_MEM_SIZE;
static int ma_algorithm = MA_FF;
static int pid_seq = 0;
static int lock_set_size = 0;

// 关键：最小碎片阈值（可运行时修改）
static int MIN_SLICE = 10;          // 设为 10：小碎片直接并给进程；设为 0：严格切分
static int AUTO_COMPACT = 1;        // 自动紧缩开关（总空闲≥需求但无单块时）

// 声明
static void clear_input(void);
static free_block_type* init_free_block(int size);
static void display_menu(void);
static int  set_mem_size(void);
static void set_algorithm(void);
static void rearrange(int alg);
static void set_min_slice(void);
static void toggle_auto_compact(void);

static void sort_free_by_addr(void);
static void sort_free_by_size_asc(void);
static void sort_free_by_size_desc(void);
static void merge_free_adjacent(void);
static void insert_free_tail(free_block_type *node);

static int  new_process(void);
static int  allocate_mem(allocated_block *ab);
static int  sum_free_memory(void);
static int  compact_memory(void);

static void kill_process(void);
static allocated_block* find_process(int pid);
static int  free_mem(allocated_block *ab);
static int  dispose(allocated_block *ab);

static void sort_alloc_by_addr(void);
static void display_mem_usage(void);
static void do_exit(void);

// 实现
static void clear_input(void){ int c; while((c=getchar())!='\n' && c!=EOF){} }

static free_block_type* init_free_block(int size){
    free_block_type *fb = (free_block_type*)malloc(sizeof(free_block_type));
    if(!fb){ printf("No mem\n"); return NULL; }
    fb->size = size; fb->start_addr = DEFAULT_MEM_START; fb->next = NULL; return fb;
}

static void display_menu(void){
    printf("\n");
    printf("1 - 设置内存大小 (默认=%d)\n", DEFAULT_MEM_SIZE);
    printf("2 - 选择内存分配算法（FF/BF/WF）\n");
    printf("3 - 创建新进程（申请内存）\n");
    printf("4 - 结束进程（回收内存）\n");
    printf("5 - 显示内存使用情况\n");
    printf("6 - 设置最小碎片阈值 MIN_SLICE（当前=%d）\n", MIN_SLICE);
    printf("7 - 切换自动紧缩（当前=%s）\n", AUTO_COMPACT ? "开启" : "关闭");
    printf("0 - 退出\n");
    printf("选择> "); fflush(stdout);
}

static int set_mem_size(void){
    if(lock_set_size){ printf("已有操作发生，不能再次设置内存大小。\n"); return 0; }
    int size; printf("总内存大小 = ");
    if(scanf("%d",&size)!=1 || size<=0){ printf("输入无效。\n"); clear_input(); return 0; }
    clear_input();
    mem_size = size;
    if(free_block){ free_block->size = mem_size; free_block->start_addr = DEFAULT_MEM_START; free_block->next = NULL; }
    else free_block = init_free_block(mem_size);
    return 1;
}

static void set_algorithm(void){
    int alg;
    printf("\t1 - First Fit (FF)\n\t2 - Best  Fit (BF)\n\t3 - Worst Fit (WF)\n选择> ");
    if(scanf("%d",&alg)!=1 || alg<1 || alg>3){ printf("输入无效。\n"); clear_input(); return; }
    clear_input(); ma_algorithm = alg; rearrange(ma_algorithm); lock_set_size = 1;
}

static void set_min_slice(void){
    int v; printf("设置 MIN_SLICE = ");
    if(scanf("%d",&v)!=1 || v<0){ printf("输入无效。\n"); clear_input(); return; }
    clear_input(); MIN_SLICE = v; printf("MIN_SLICE 已设为 %d\n", MIN_SLICE);
}

static void toggle_auto_compact(void){
    AUTO_COMPACT ^= 1;
    printf("自动紧缩已%s。\n", AUTO_COMPACT ? "开启" : "关闭");
}

static void rearrange(int alg){
    switch(alg){
        case MA_FF: sort_free_by_addr(); break;
        case MA_BF: sort_free_by_size_asc(); break;
        case MA_WF: sort_free_by_size_desc(); break;
    }
}

static void insert_free_tail(free_block_type *node){
    if(!free_block){ free_block = node; return; }
    free_block_type *p = free_block; while(p->next) p = p->next; p->next = node;
}

static void sort_free_by_addr(void){
    free_block_type *sorted=NULL,*cur=free_block;
    while(cur){
        free_block_type *n=cur->next;
        if(!sorted || cur->start_addr < sorted->start_addr){ cur->next=sorted; sorted=cur; }
        else{ free_block_type *p=sorted; while(p->next && p->next->start_addr<=cur->start_addr) p=p->next;
              cur->next=p->next; p->next=cur; }
        cur=n;
    }
    free_block=sorted;
}

static void sort_free_by_size_asc(void){
    free_block_type *sorted=NULL,*cur=free_block;
    while(cur){
        free_block_type *n=cur->next;
        if(!sorted || cur->size<sorted->size || (cur->size==sorted->size && cur->start_addr<sorted->start_addr)){
            cur->next=sorted; sorted=cur;
        }else{
            free_block_type *p=sorted;
            while(p->next && (p->next->size<cur->size || (p->next->size==cur->size && p->next->start_addr<=cur->start_addr))) p=p->next;
            cur->next=p->next; p->next=cur;
        }
        cur=n;
    }
    free_block=sorted;
}

static void sort_free_by_size_desc(void){
    free_block_type *sorted=NULL,*cur=free_block;
    while(cur){
        free_block_type *n=cur->next;
        if(!sorted || cur->size>sorted->size || (cur->size==sorted->size && cur->start_addr<sorted->start_addr)){
            cur->next=sorted; sorted=cur;
        }else{
            free_block_type *p=sorted;
            while(p->next && (p->next->size>cur->size || (p->next->size==cur->size && p->next->start_addr<=cur->start_addr))) p=p->next;
            cur->next=p->next; p->next=cur;
        }
        cur=n;
    }
    free_block=sorted;
}

static void merge_free_adjacent(void){
    if(!free_block) return;
    sort_free_by_addr();
    free_block_type *cur=free_block;
    while(cur && cur->next){
        if(cur->start_addr + cur->size == cur->next->start_addr){
            free_block_type *v=cur->next; cur->size += v->size; cur->next = v->next; free(v);
        }else cur=cur->next;
    }
}

static int new_process(void){
    allocated_block *ab=(allocated_block*)malloc(sizeof(allocated_block));
    if(!ab){ printf("控制块内存不足。\n"); return -1; }
    ab->next=NULL; pid_seq++; snprintf(ab->process_name,PROCESS_NAME_LEN,"PROCESS-%02d",pid_seq); ab->pid=pid_seq;
    int size; printf("为 %s 申请内存大小: ", ab->process_name);
    if(scanf("%d",&size)!=1 || size<=0){ printf("输入无效，取消。\n"); clear_input(); free(ab); return -1; }
    clear_input(); ab->size=size;

    int ret = allocate_mem(ab);
    if(ret==1){ ab->next=allocated_block_head; allocated_block_head=ab; lock_set_size=1; return 1; }
    printf("分配失败。\n"); free(ab); return -1;
}

// 这里体现 MIN_SLICE：若剩余 < MIN_SLICE，整块给出去（减少过小外碎片）
static int allocate_mem(allocated_block *ab){
    free_block_type *pre=NULL,*cur=free_block;
    while(cur){
        if(cur->size >= ab->size){
            ab->start_addr = cur->start_addr;
            int leftover = cur->size - ab->size;
            if(leftover == 0){
                // 恰好
                if(pre) pre->next = cur->next; else free_block = cur->next;
                free(cur);
            }else if(leftover < MIN_SLICE){
                // 阈值内：整块给
                ab->size = cur->size;
                if(pre) pre->next = cur->next; else free_block = cur->next;
                free(cur);
            }else{
                // 正常切分
                cur->start_addr += ab->size;
                cur->size      -= ab->size;
            }
            rearrange(ma_algorithm);
            return 1;
        }
        pre=cur; cur=cur->next;
    }
    // 单块不够：是否自动紧缩
    if(AUTO_COMPACT && sum_free_memory() >= ab->size){
        if(compact_memory()==1) return allocate_mem(ab);
    }
    return -1;
}

static int sum_free_memory(void){ int s=0; for(free_block_type*p=free_block;p;p=p->next) s+=p->size; return s; }

static void sort_alloc_by_addr(void){
    allocated_block *sorted=NULL,*cur=allocated_block_head;
    while(cur){
        allocated_block *n=cur->next;
        if(!sorted || cur->start_addr<sorted->start_addr){ cur->next=sorted; sorted=cur; }
        else{ allocated_block *p=sorted; while(p->next && p->next->start_addr<=cur->start_addr) p=p->next;
              cur->next=p->next; p->next=cur; }
        cur=n;
    }
    allocated_block_head=sorted;
}

static int compact_memory(void){
    sort_alloc_by_addr();
    int next = DEFAULT_MEM_START;
    for(allocated_block*ab=allocated_block_head;ab;ab=ab->next){
        if(ab->start_addr != next) ab->start_addr = next;
        next += ab->size;
    }
    // 重建空闲链为单大块
    free_block_type *p=free_block; while(p){ free_block_type *t=p->next; free(p); p=t; } free_block=NULL;
    int free_size = mem_size - (next - DEFAULT_MEM_START);
    if(free_size > 0){
        free_block_type *tail=(free_block_type*)malloc(sizeof(free_block_type));
        if(!tail) return -1;
        tail->start_addr = next; tail->size = free_size; tail->next = NULL; free_block = tail;
    }
    rearrange(ma_algorithm);
    return 1;
}

static void kill_process(void){
    int pid; printf("结束进程，pid = ");
    if(scanf("%d",&pid)!=1){ printf("输入无效。\n"); clear_input(); return; }
    clear_input();
    allocated_block *ab=find_process(pid);
    if(!ab){ printf("未找到该 pid。\n"); return; }
    free_mem(ab); dispose(ab); lock_set_size=1;
}

static allocated_block* find_process(int pid){
    for(allocated_block*p=allocated_block_head;p;p=p->next) if(p->pid==pid) return p;
    return NULL;
}

static int free_mem(allocated_block *ab){
    free_block_type *node=(free_block_type*)malloc(sizeof(free_block_type));
    if(!node) return -1;
    node->size = ab->size; node->start_addr = ab->start_addr; node->next = NULL;
    insert_free_tail(node);
    sort_free_by_addr();
    merge_free_adjacent();
    rearrange(ma_algorithm);
    return 1;
}

static int dispose(allocated_block *free_ab){
    if(!free_ab) return -1;
    if(free_ab == allocated_block_head){ allocated_block_head = allocated_block_head->next; free(free_ab); return 1; }
    allocated_block *pre=allocated_block_head,*cur=allocated_block_head->next;
    while(cur && cur!=free_ab){ pre=cur; cur=cur->next; }
    if(cur==free_ab){ pre->next=cur->next; free(cur); return 2; }
    return -1;
}

static void display_mem_usage(void){
    printf("----------------------------------------------------------\n");
    printf("Free Memory:\n");
    printf("%20s %20s\n","start_addr","size");
    for(free_block_type*f=free_block;f;f=f->next) printf("%20d %20d\n",f->start_addr,f->size);

    printf("\nUsed Memory:\n");
    printf("%10s %20s %12s %10s\n","PID","ProcessName","start_addr","size");
    for(allocated_block*ab=allocated_block_head;ab;ab=ab->next)
        printf("%10d %20s %12d %10d\n",ab->pid,ab->process_name,ab->start_addr,ab->size);
    printf("----------------------------------------------------------\n");
}

static void do_exit(void){
    allocated_block *a=allocated_block_head; while(a){ allocated_block*t=a->next; free(a); a=t; } allocated_block_head=NULL;
    free_block_type *f=free_block; while(f){ free_block_type*t=f->next; free(f); f=t; } free_block=NULL;
}

// main
int main(void){
    pid_seq=0; free_block=init_free_block(mem_size);
    for(;;){
        display_menu();
        int choice; if(scanf("%d",&choice)!=1){ clear_input(); continue; }
        clear_input();
        switch(choice){
            case 1: set_mem_size(); break;
            case 2: set_algorithm(); break;
            case 3: new_process(); break;
            case 4: kill_process(); break;
            case 5: display_mem_usage(); break;
            case 6: set_min_slice(); break;
            case 7: toggle_auto_compact(); break;
            case 0: do_exit(); return 0;
            default: printf("无效选择。\n"); break;
        }
    }
}
