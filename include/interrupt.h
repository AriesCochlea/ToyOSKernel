#pragma once
#include "stdint.h"



//注意：CPU进入中断后会自动把标志寄存器的IF位置0,即屏蔽可屏蔽中断,也即自动关中断




void idt_init();                                      //完成有关中断的所有初始化工作

void register_handler(uint8_t vec_no, void* function);//注册中断处理函数：把向量号为vec_no的中断处理函数指针function注册到idt_table[]中




enum intr_status {
    INTR_OFF,
    INTR_ON
};
enum intr_status intr_get_status(); 
enum intr_status intr_set_status(enum intr_status);//返回旧的enum intr_status
enum intr_status intr_enable();                    //返回旧的enum intr_status
enum intr_status intr_disable();                   //返回旧的enum intr_status

