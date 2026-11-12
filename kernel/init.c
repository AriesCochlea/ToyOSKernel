#include "interrupt.h"
#include "timer.h"
#include "memory.h"
#include "thread.h"
#include "keyboard.h"
#include "tss.h"
#include "syscall-init.h"
#include "ide.h"
#include "fs.h"



void init_all(){
	idt_init();    //初始化中断描述符表IDT、注册通用中断处理函数（伪中断处理函数）、初始化可编程中断控制器8259A、最后加载IDT
	timer_init();  //注册时钟中断处理函数
	mem_init();    //根据检测到的可用内存，初始化内核物理内存池、用户物理内存池、内核虚拟内存池
	thread_init(); //初始化线程总队列、就绪队列，为主线程创建TCB，并创建init进程和idle线程
	keyboard_init(); //初始化键盘缓冲区，注册键盘中断处理函数
	tss_init();    //在GDT中添加TSS描述符和用户进程使用的代码段、数据段描述符
	syscall_init();//注册系统调用的子功能处理函数


	intr_enable(); //中断在loader.asm中关闭了，需要打开。后面的ide_init需要用到硬盘中断
	ide_init();    //初始化硬盘，扫描硬盘分区
	filesys_init();//在硬盘分区上创建或挂载文件系统
}
