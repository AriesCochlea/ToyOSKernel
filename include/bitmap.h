#ifndef __LIB_KERNEL_BITMAP_H
#define __LIB_KERNEL_BITMAP_H


#include "stdint.h"

#define BITMAP_MASK 1

struct bitmap
{
    uint32_t btmp_bytes_len;
    uint8_t* bits;
};

void bitmap_init(struct bitmap* btmp);//将位图初始化为全0，即全部空闲
int bitmap_scan_test(struct bitmap* btmp,uint32_t bit_idx);
int  bitmap_scan(struct bitmap* btmp,uint32_t cnt);//在位图中申请连续的cnt个空闲位并将它们置1，若成功返回其起始下标，若失败返回-1
void bitmap_set(struct bitmap* btmp,uint32_t bit_idx,int8_t value);

#endif
