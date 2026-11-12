#include "stdint.h"
#include "interrupt.h"
#include "thread.h"
#include "ioPort.h"
#include "print.h"



#define	 RPL0  0
#define	 RPL1  1
#define	 RPL2  2
#define	 RPL3  3

#define TI_GDT 0
#define TI_LDT 1

#define SELECTOR_K_CODE	   ((1 << 3) + (TI_GDT << 2) + RPL0)
#define SELECTOR_K_DATA	   ((2 << 3) + (TI_GDT << 2) + RPL0)
#define SELECTOR_K_STACK   SELECTOR_K_DATA 
#define SELECTOR_K_GS	   ((3 << 3) + (TI_GDT << 2) + RPL0)

//--------------   IDT描述符属性  ------------
#define	 IDT_DESC_P	 1 
#define	 IDT_DESC_DPL0   0
#define	 IDT_DESC_DPL3   3
#define	 IDT_DESC_32_TYPE     0xE
#define	 IDT_DESC_ATTR_DPL0  ((IDT_DESC_P << 7) + (IDT_DESC_DPL0 << 5) + IDT_DESC_32_TYPE)
#define	 IDT_DESC_ATTR_DPL3  ((IDT_DESC_P << 7) + (IDT_DESC_DPL3 << 5) + IDT_DESC_32_TYPE)



#define PIC_M_CTRL 0x20	       // 这里用的可编程中断控制器是8259A,主片的控制端口是0x20
#define PIC_M_DATA 0x21	       // 主片的数据端口是0x21
#define PIC_S_CTRL 0xa0	       // 从片的控制端口是0xa0
#define PIC_S_DATA 0xa1	       // 从片的数据端口是0xa1




//本项目实现的中断：
//1.时钟中断，中断号0x20，用于实现进程调度
//2.键盘中断，中断号0x21，用于实现键盘驱动
//3.硬盘中断，中断号0x2e，用于实现硬盘驱动
//4.软中断，中断号0x80，用于实现系统调用

#define IDT_DESC_CNT 0x81      // 目前总共支持的中断向量号为：0～0x80
#define IDT_ENTRY_CNT 0x30     // interrupt.asm的intr_entry_table中定义了0～0x2f共0x30个中断入口



/*中断门描述符*/
struct gate_desc {
   unsigned short    func_offset_low_word;
   unsigned short    selector;
   unsigned char     dcount;
   unsigned char     attribute;
   unsigned short    func_offset_high_word;
};
static struct gate_desc idt[IDT_DESC_CNT];   //IDT：中断描述符表，本质上就是个中断门描述符数组
char* intr_name[IDT_DESC_CNT];		         //用于保存中断的名字
void* idt_table[IDT_DESC_CNT];               //C版本中断处理函数入口地址表，见interrupt.asm中的call [idt_table + %1*4] 
extern void* intr_entry_table[IDT_ENTRY_CNT];//汇编版本中断处理函数入口地址表（在interrupt.asm中），依次执行保存寄存器、idt_table[i]()、中断返回
                                             //即intr_entry_table[i]()是对idt_table[i]()的封装
extern void syscall_handler(void);           //0x80号中断（系统调用）的处理函数，在interrupt.asm中


//注册中断处理函数：把向量号为vec_no的中断处理函数指针function注册到idt_table[]中
void register_handler(uint8_t vec_no, void* function)
{
    idt_table[vec_no] = function;
}



/* 创建中断门描述符 */
static void make_idt_desc(struct gate_desc* p, unsigned char attr, void* function){ 
   p->func_offset_low_word = (unsigned int)function & 0x0000FFFF;
   p->selector = SELECTOR_K_CODE;
   p->dcount = 0;
   p->attribute = attr;
   p->func_offset_high_word = ((unsigned int)function & 0xFFFF0000) >> 16;
}
/*初始化中断描述符表IDT*/
static void idt_desc_init(){
   for (int i = 0; i < IDT_ENTRY_CNT; i++) {
      make_idt_desc(&idt[i], IDT_DESC_ATTR_DPL0, intr_entry_table[i]);
   }
   //单独处理系统调用：系统调用对应的中断门dpl为3，用户进程才能用int 0x80进入，中断处理程序为单独的syscall_handler
   make_idt_desc(&idt[0x80], IDT_DESC_ATTR_DPL3, syscall_handler);
}








//通用的中断处理函数
static void general_intr_handle(unsigned int vec_nr) {
   if(vec_nr >= IDT_DESC_CNT)
      return;
    if (vec_nr == 0x27 || vec_nr == 0x2f) {	//0x2f是从片8259A上的最后一个irq引脚，保留
       return;		                           //IRQ7和IRQ15会产生伪中断(spurious interrupt)，无须处理
    }

   set_cursor(0);                            //重置光标为屏幕左上角
   int cursor_pos = 0;
   while((cursor_pos++) < 320)			      //一行80字，打印4行空格
   	put_char(' ');			
   
   set_cursor(0);                            //重置光标为屏幕左上角
   put_str("!!!!!!            exception message begin            !!!!!!\n");
   set_cursor(88);					            //第2行第8个字开始打印
   put_str("int vector: 0x");               
   put_16uint(vec_nr);                       //按16进制打印
   put_char(' ');
   put_str(intr_name[vec_nr]);               //打印中断名称
   if(vec_nr == 14)                          //如果是缺页异常，则将缺失的虚拟地址打印出来并悬停
   {
   	int page_fault_vaddr = 0;
   	asm("movl %%cr2, %0" : "=r" (page_fault_vaddr));   //把虚拟地址 出错的放到了这个变量里面
	   put_str("\npage fault vaddr is ");
	   put_16uint(page_fault_vaddr);
   }
   put_str("\n!!!!!!            exception message end              !!!!!!\n");
   
   while(1);   
}


//通用中断处理函数初始化注册
static void interrupt_init() {
   for (int i = 0; i < IDT_DESC_CNT; i++) {
      idt_table[i] = general_intr_handle;	
      intr_name[i] = "Reserved";
   }
   intr_name[0] = "#DE Divide Error";
   intr_name[1] = "#DB Debug Exception";
   intr_name[2] = "NMI Interrupt";
   intr_name[3] = "#BP Breakpoint Exception";
   intr_name[4] = "#OF Overflow Exception";
   intr_name[5] = "#BR BOUND Range Exceeded Exception";
   intr_name[6] = "#UD Invalid Opcode Exception";
   intr_name[7] = "#NM Device Not Available Exception";
   intr_name[8] = "#DF Double Fault Exception";
   intr_name[9] = "#MF Coprocessor Segment Overrun";
   intr_name[10] = "#TS Invalid TSS Exception";
   intr_name[11] = "#NP Segment Not Present";
   intr_name[12] = "#SS Stack Fault Exception";
   intr_name[13] = "#GP General Protection Exception";
   intr_name[14] = "#PF Page-Fault Exception";
   //intr_name[15] 第15项是Intel保留项，未使用
   intr_name[16] = "#MF x87 FPU Floating-Point Error";
   intr_name[17] = "#AC Alignment Check Exception";
   intr_name[18] = "#MC Machine-Check Exception";
   intr_name[19] = "#XF SIMD Floating-Point Exception";
}







/* 初始化可编程中断控制器8259A */
static void pic_init() {

   /* 初始化主片 */
   outb (PIC_M_CTRL, 0x11);   // ICW1: 边沿触发,级联8259, 需要ICW4.
   outb (PIC_M_DATA, 0x20);   // ICW2: 设置起始中断向量号为0x20,也就是IR[0-7]为 0x20~0x27.
                              // ICW2专用于设置8259A的起始中断向量号，由于中断向量号0～ 31已经被占用或保留，从32起才可用
                              // 在开机之后的实模式下，8259A的IRQO～7己经被BIOS分配了0x8～0xf的中断向量号
                              // 而保护模式下，中断向量号0x8～0xf已经被CPU分配给各种异常和不可屏蔽中断，所以要重新为8259A芯片上的IRQ接口分配中断向量号
   outb (PIC_M_DATA, 0x04);   // ICW3: IR2接从片.
   outb (PIC_M_DATA, 0x01);   // ICW4: 8086模式, 正常EOI

   /* 初始化从片 */
   outb (PIC_S_CTRL, 0x11);    // ICW1: 边沿触发,级联8259, 需要ICW4.
   outb (PIC_S_DATA, 0x28);    // ICW2: 设置起始中断向量号为0x28,也就是IR[8-15]为0x28~0x2F
                               // 主片的中断向量号是Ox20～Ox27，从片从0x28开始
   outb (PIC_S_DATA, 0x02);    // ICW3: 设置从片连接到主片的IR2引脚
   outb (PIC_S_DATA, 0x01);    // ICW4: 8086模式, 正常EOI（正常结束中断）
   
   
   //打开主片上IRQ0（时钟中断）、IRQ1（键盘中断）、IRQ2（级联从片，必须打开，否则无法响应从片上的中断）
   outb (PIC_M_DATA, 0xf8);    //即11111000b，第0、1、2位为0，表示不屏蔽。其他位都是1，表示都屏蔽
   //打开从片上的IRQ14（硬盘中断），屏蔽从片上的其他中断
   outb (PIC_S_DATA, 0xbf);    //即10111111b
}








//完成有关中断的所有初始化工作
void idt_init() {
   //put_str("idt_init start\n");
   idt_desc_init();	
   interrupt_init();
   pic_init();	

   //加载idt 
   unsigned long long idt_operand = ((sizeof(idt) - 1) | ((unsigned long long)(unsigned int)idt << 16));
   asm volatile("lidt %0" : : "m" (idt_operand));
   //put_str("idt_init end\n");
}






enum intr_status intr_get_status() {
    unsigned int eflags = 0;
    asm volatile ("pushfl; popl %0" : "=g"(eflags));
    return (0x00000200 & eflags) ? INTR_ON : INTR_OFF; //0x00000200表示IF位为1
}

enum intr_status intr_set_status(enum intr_status status) {
	return ((status == INTR_ON) ? intr_enable() : intr_disable());
}

enum intr_status intr_enable() {
	enum intr_status old = intr_get_status();
    if (INTR_ON == old)
        return INTR_ON;
    asm volatile ("sti");
    return INTR_OFF;
}

enum intr_status intr_disable() {
	enum intr_status old = intr_get_status();
    if (INTR_OFF == old) 
    	return INTR_OFF;
    asm volatile ("cli" : : : "memory"); 
    return INTR_ON;
}


