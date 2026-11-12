//键盘驱动

#pragma once
#include "ioqueue.h"

void keyboard_init(void);
extern struct ioqueue kbd_buf;   //键盘缓冲区
