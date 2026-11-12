//时钟中断

#include "timer.h"
#include "interrupt.h"
#include "thread.h"
#include "print.h"
#include "debug.h"
#include "global.h"


//本项目没有重新设置8253定时器，时钟中断的频率沿用BIOS的默认设置：1193180Hz / 65536 ≈ 18.2Hz，即每秒约18次
#define IRQ0_FREQUENCY 18
#define mil_seconds_per_intr (1000 / IRQ0_FREQUENCY)   //每次时钟中断间隔的毫秒数



unsigned int total_ticks=0; //时钟中断发生的总次数，相当于系统运行时间。下面的elapsed_ticks是当前线程已经运行的总时钟数，ticks是该线程此次允许运行的时钟数（即时间片余量）。

//时钟中断的处理函数
void intr_timer_handler(void)
{   //enum intr_status old = intr_disable();               //CPU进入中断后会push eflags并自动将标志寄存器的IF位置0（关中断）来禁止中断嵌套，无需手动关中断
    struct task_struct* cur_thread = running_thread();     //得到当前正在运行的线程的pcb指针
    ASSERT(cur_thread->stack_magic == 0x20220701);	       //检测栈是否溢出

    ++total_ticks;
    ++(cur_thread->elapsed_ticks);                          //当前线程已经运行的总时钟数增加1
    if(cur_thread->ticks==0)
    	schedule();                                         //时间片用完后重新调度
    else
    	--(cur_thread->ticks);                              //当前线程的时间片减1


    //intr_enable();                                        //中断iretd返回时会pop eflags即再次开中断，无需手动开中断
}



void timer_init(void)
{
    put_str("timer_init start!\n");
    register_handler(0x20,intr_timer_handler);	            //注册时间中断处理函数，中断向量号为0x20
    put_str("timer_init done!\n");
    return;
}



//以tick为单位的sleep，任何时间形式的sleep会转换成此ticks形式
static void ticks_to_sleep(uint32_t sleep_ticks)
{
    uint32_t start_tick = total_ticks;
    while(total_ticks - start_tick < sleep_ticks)           //若间隔的ticks数不够便让出cpu
        thread_yield();
}

//以毫秒为单位的sleep，1秒 = 1000毫秒
void mtime_sleep(uint32_t m_seconds)
{
    uint32_t sleep_ticks = DIV_ROUND_UP(m_seconds, mil_seconds_per_intr);
    ASSERT(sleep_ticks > 0);
    ticks_to_sleep(sleep_ticks);
}
