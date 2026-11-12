//任务状态段TSS：本项目只用它来保存用户进程陷入内核时使用的0特权级栈

#pragma once
#include "thread.h"

void update_tss_esp(struct task_struct* pthread);
void tss_init(void);
