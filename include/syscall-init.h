//内核态的系统调用初始化：注册系统调用子功能处理函数

#pragma once
#include "stdint.h"

void syscall_init(void);
uint32_t sys_getpid(void);
