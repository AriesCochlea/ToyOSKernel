//用户进程

#pragma once
#include "thread.h"
#include "stdint.h"

#define default_prio 31                           //用户进程的默认优先级
#define USER_STACK3_VADDR  (0xc0000000 - 0x1000)  //用户进程3特权级栈所在页的起始虚拟地址
#define USER_VADDR_START 0x8048000                //用户进程虚拟地址空间的起始地址（Linux用户程序入口地址的惯例）

void process_execute(void* filename, char* name);
void start_process(void* filename_);
void process_activate(struct task_struct* p_thread);
void page_dir_activate(struct task_struct* p_thread);
uint32_t* create_page_dir(void);
void create_user_vaddr_bitmap(struct task_struct* user_prog);
