//供内核使用的格式化输出

#pragma once
#include "stdint.h"

void printk(const char* format, ...);
