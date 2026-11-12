//供用户进程使用的assert和panic（内核使用的是debug.h中的ASSERT和PANIC）

#include "assert.h"
#include "stdio.h"

void user_spin(char* filename, int line, const char* func, const char* condition) {
    printf("\n\n\n\nfilename %s\nline %d\nfunction %s\ncondition %s\n", filename, line, func, condition);
    while(1);
}
