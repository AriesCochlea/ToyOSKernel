//fork系统调用

#pragma once
#include "thread.h"

pid_t sys_fork(void);   //fork子进程，内核线程不可直接调用
