#include <time.h>
#include <stdio.h>
#include "util.h"

void ts_now(uint32_t* out){ if(out) *out = (uint32_t)time(NULL); }

void human_time(uint32_t t, char* out, size_t n){
    time_t tt = t; struct tm* tmv = localtime(&tt);
    if(!tmv){ if(n) out[0]='\0'; return; }
    strftime(out, n, "%Y-%m-%d %H:%M:%S", tmv);
}
