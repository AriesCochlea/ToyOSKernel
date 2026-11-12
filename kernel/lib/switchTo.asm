;线程切换：void switch_to(struct task_struct* cur, struct task_struct* next);
;保存当前线程cur的上下文环境（ABI规定的由被调函数保存的寄存器），再恢复下一个线程next的上下文环境，对应thread.h中的struct thread_stack

[bits 32]
section .text
global switch_to
switch_to:
   ;栈中此处是返回地址
   push esi
   push edi
   push ebx
   push ebp

   mov eax, [esp + 20]           ;得到栈中的参数cur, cur = [esp+20]
   mov [eax], esp                ;保存栈顶指针esp到task_struct的self_kstack字段，self_kstack在task_struct中的偏移为0，所以直接往cur开头处存4字节便可

;------------------  以上是备份当前线程的环境，下面是恢复下一个线程的环境  ----------------
   mov eax, [esp + 24]           ;得到栈中的参数next, next = [esp+24]
   mov esp, [eax]                ;pcb的第一个成员是self_kstack成员，用来记录0级栈顶指针，用来上cpu时恢复0级栈，0级栈中保存了进程或线程所有信息，包括3级栈指针
   pop ebp
   pop ebx
   pop edi
   pop esi
   ret                           ;返回到上面switch_to下面的那句注释的返回地址，未由中断进入，第一次执行时会返回到kernel_thread
