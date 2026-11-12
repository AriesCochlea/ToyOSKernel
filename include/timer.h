//时钟中断

#ifndef __DEVICE_TIME_H
#define __DEVICE_TIME_H

#include "stdint.h"

void intr_timer_handler(void);
void timer_init(void);
void mtime_sleep(uint32_t m_seconds);   //以毫秒为单位的sleep

#endif
