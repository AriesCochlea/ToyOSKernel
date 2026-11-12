//供用户进程使用的assert和panic（内核使用的是debug.h中的ASSERT和PANIC）

#pragma once

void user_spin(char* filename, int line, const char* func, const char* condition);
#define panic(...) user_spin(__FILE__, __LINE__, __func__, __VA_ARGS__)

#ifdef NDEBUG
    #define assert(CONDITION) ((void)0)
#else
    #define assert(CONDITION)  \
        if (!(CONDITION)) {     \
            panic(#CONDITION);   \
        }
#endif
