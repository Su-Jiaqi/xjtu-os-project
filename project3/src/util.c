#include <time.h>
#include <stdio.h>
#include "fs.h"

void ts_now(uint32_t* out){ if(out) *out = (uint32_t)time(NULL); }

void human_time(uint32_t t, char* out, size_t n){
    time_t tt = t; struct tm* tmv = localtime(&tt);
    if(!tmv){ if(n) out[0]='\0'; return; }
    strftime(out, n, "%Y-%m-%d %H:%M:%S", tmv);
}

void mode_to_str(uint16_t mode, char out[11]){
    out[0] = ((mode & 0170000)==0040000)? 'd' : '-';
    out[1] = (mode & 0400)?'r':'-';
    out[2] = (mode & 0200)?'w':'-';
    out[3] = (mode & 0100)?'x':'-';
    out[4] = (mode & 0040)?'r':'-';
    out[5] = (mode & 0020)?'w':'-';
    out[6] = (mode & 0010)?'x':'-';
    out[7] = (mode & 0004)?'r':'-';
    out[8] = (mode & 0002)?'w':'-';
    out[9] = (mode & 0001)?'x':'-';
    out[10]='\0';
}
