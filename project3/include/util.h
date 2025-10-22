#ifndef MINI_EXT2_UTIL_H
#define MINI_EXT2_UTIL_H
#include <stdint.h>
#include <stddef.h>

void ts_now(uint32_t* out);
void human_time(uint32_t t, char* out, size_t n);

#endif
