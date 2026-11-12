//exec系统调用：加载并运行文件系统中的用户程序

#pragma once
#include "stdint.h"

int32_t sys_execv(const char* path, const char* argv[]);
