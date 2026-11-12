#include "thread.h"
#include "stdint.h"
#include "debug.h"
#include "print.h"
#include "string.h"   //memset
#include "memory.h"   //分配页需要
#include "interrupt.h"
#include "global.h"
#include "sync.h"
#include "process.h"
#include "stdio.h"
#include "fs.h"
#include "file.h"


/* pid的位图，最大支持1024个pid */
uint8_t pid_bitmap_bits[128] = {0};

/* pid池 */
struct pid_pool{
    struct bitmap pid_bitmap;  // pid位图
    uint32_t pid_start;        // 起始pid
    struct lock pid_lock;      // 分配pid锁
}pid_pool;


struct task_struct*  main_thread;          //主线程的task_struct即PCB
struct task_struct*  idle_thread;          //idle线程：系统空闲时运行
struct list    thread_ready_list;		   //就绪队列
struct list      thread_all_list;		   //所有线程（进程）队列
static struct list_elem* thread_tag;       // 用于保存队列中的线程结点


extern void switch_to(struct task_struct* cur, struct task_struct* next); //见kernel/lib/switchTo.asm
extern void init(void);                                                   //init进程，见main.c



//系统空闲时运行的线程
static void idle(void* arg UNUSED){
    while(1){
        thread_block(TASK_BLOCKED);
        //执行hlt时必须要保证目前处在开中断的情况下
        asm volatile ("sti; hlt" : : : "memory");
    }
}



// 获取当前pcb指针
// 我们线程所在的esp 肯定是在 我们get得到的那一页内存 pcb页上下浮动 但是我们的pcb的最起始位置是整数的 除去后面的12位
// 那么我们对前面的取 & 则可以得到 我们的地址所在地
struct task_struct* running_thread()
{
    uint32_t esp;
    asm ("mov %%esp, %0" : "=g" (esp));               //取esp整数部分，即pcb起始地址＊
    return (struct task_struct*)(esp & 0xfffff000);
}



//初始化pid池
static void pid_pool_init(void){
    pid_pool.pid_start = 1;
    pid_pool.pid_bitmap.bits = pid_bitmap_bits;
    pid_pool.pid_bitmap.btmp_bytes_len = 128;
    bitmap_init(&pid_pool.pid_bitmap);
    lock_init(&pid_pool.pid_lock);
}

//分配pid
static pid_t allocate_pid(void){
    lock_acquire(&pid_pool.pid_lock);
    int32_t bit_idx = bitmap_scan(&pid_pool.pid_bitmap, 1);
    bitmap_set(&pid_pool.pid_bitmap, bit_idx, 1);
    lock_release(&pid_pool.pid_lock);
    return (bit_idx + pid_pool.pid_start);
}

//释放pid
void release_pid(pid_t pid){
    lock_acquire(&pid_pool.pid_lock);
    int32_t bit_idx = pid - pid_pool.pid_start;
    bitmap_set(&pid_pool.pid_bitmap, bit_idx, 0);
    lock_release(&pid_pool.pid_lock);
}

//fork进程时为其分配pid。因为allocate_pid已经是静态的，别的文件无法调用，故定义fork_pid函数来封装一下
pid_t fork_pid(void){
    return allocate_pid();
}




//初始化线程基本信息
void init_thread(struct task_struct* pthread,char* name,int prio)
{
    memset(pthread,0,sizeof(*pthread)); //pcb位置清0
    pthread->pid = allocate_pid();
    strcpy(pthread->name,name);

    if(pthread == main_thread)
    	pthread->status = TASK_RUNNING;                              //我们的主线程肯定是在运行的
    else
    	pthread->status = TASK_READY;				                 //放到就绪队列里面

    pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE); //栈顶初始化为TCB或PCB所在页的顶部
                                                                     //在switch_to()函数中也会改变，switchTo.asm文件：mov [eax], esp
    pthread->priority = prio;
    pthread->ticks = prio;                                           //和特权级 相同的时间片
    pthread->elapsed_ticks = 0;
    pthread->pgdir = NULL;                                           //线程没有单独的地址

    /* 标准输入输出先空出来 */
    pthread->fd_table[0] = 0;
    pthread->fd_table[1] = 1;
    pthread->fd_table[2] = 2;
    /* 其余的全置为-1 */
    uint8_t fd_idx = 3;
    while(fd_idx < MAX_FILES_OPEN_PER_PROC){
        pthread->fd_table[fd_idx] = -1;
        fd_idx++;
    }
    pthread->cwd_inode_nr = 0;                                       //以根目录做为默认工作路径
    pthread->parent_pid = -1;                                        //-1表示没有父进程
    pthread->stack_magic = 0x20220701;                               //设置的魔数 检测是否越界限。我写的是日期2022.07.01
}





static void kernel_thread(thread_func* function,  void* func_arg){
	intr_enable();     //第一次上CPU运行时是关中断的状态，所以要开中断，允许该线程被时钟中断调度
    function(func_arg);
}

void thread_create(struct task_struct* pthread,thread_func function,void* func_arg)
{
    pthread->self_kstack -= sizeof(struct intr_stack);   //减去中断栈的空间，且中断栈的内容全为0————调用get_kernel_pages()时会清零（因为线程首次运行）
    pthread->self_kstack -= sizeof(struct thread_stack); //减去线程栈的空间，将pthread->self_kstack指向thread_stack的基址
    /*以上两个子栈都属于线程的内核栈的一部分。只有thread_stack在该线程（肯定不是主线程）第一次上CPU运行时被用到，
      因为该线程第一次执行是通过时钟中断处理函数中的schedule()函数里的switch_to()函数的ret返回指令调用的。
      switch_to()里需要恢复5个寄存器的值，以下代码就是在提供可供“恢复”的寄存器值。switch_to()能成功返回，但调用它的中断处理函数不会返回，因为esp变了。
      所以目前的中断栈是形同虚设的。将来实现用户进程时，会将用户进程的初始信息放在中断栈中。
    */
    struct thread_stack* kthread_stack = (struct thread_stack*)pthread->self_kstack;
    kthread_stack->eip = kernel_thread;                 //由kernel_thread执行function。注意分析eip在代码段（而不是数据段和栈段）里的执行流
    kthread_stack->function = function;
    kthread_stack->func_arg = func_arg;
    kthread_stack->esi = 0;
    kthread_stack->edi = 0;
    kthread_stack->ebx = 0;
    kthread_stack->ebp = 0;

}






struct task_struct* thread_start(char* name,  int prio,  thread_func function,  void* func_arg){
    struct task_struct* thread = (struct task_struct*) get_kernel_pages(1);
    //分配一个4KB自然页，且只取低端sizeof(struct task_struct)大小的空间当作task_struct
    //顶端分配给了该线程第一次被调度上cpu时使用的中断栈和内存栈，中间不足4KB的空间就是该线程的内核栈栈顶可以向下扩展的范围
    //TCB或PCB都位于内核空间，包括用户进程的TCB或pcb也是在内核空间。本项目的主线程的TCB或PCB在低端1MB以内，其他线程的TCB或PCB在低端1MB以外
	if(thread==NULL)
		return NULL;
    init_thread(thread,name,prio);
    thread_create(thread,function,func_arg);

    ASSERT(!elem_find(&thread_ready_list,&(thread->general_tag)));     //之前不应该在就绪队列里面
    list_append(&thread_ready_list,&(thread->general_tag));

    ASSERT(!elem_find(&thread_all_list,&(thread->all_list_tag)));
    list_append(&thread_all_list,&(thread->all_list_tag));

    return thread;
}






//为主线程创建TCB
static void make_main_thread(){
    main_thread = running_thread();    				      //得到main_thread 的pcb指针
    /*即：
    unsigned int esp;                                     //定义此变量前栈顶指针为0xc009f000，定义此变量后的栈顶指针变为0xc009f000-4
    asm ("mov %%esp,  %0" : "=g"(esp));                   //将寄存器%esp的值输出到C变量esp中
    main_thread = (struct task_struct*)(esp & 0xfffff000);//取当前栈顶指针（0xc009f000-4）的高20位作为主线程的PCB或TCB（占一页）基址0xc009e000
                                                          //直接写main_thread = (struct task_struct*)0xc009e000;也行
                                                          //主线程的TCB放在物理低端1MB以内，而其他线程的TCB创建后放在物理低端1MB以外的广阔空间
    */

    init_thread(main_thread,"main",32);
    //主线程不需要在TCB或PCB所在页的顶部预留中断栈和内核栈的空间，而新创建的线程需要（见thread_start()函数）；
    //其他时候，主线程和新线程发生中断或线程切换时，保存环境的地方取决于当时的栈顶esp地址


    ASSERT(!elem_find(&thread_all_list, &(main_thread->all_list_tag)));
    list_append(&thread_all_list, &(main_thread->all_list_tag));
}






void schedule(){
    ASSERT(intr_get_status() == INTR_OFF);
	struct task_struct* cur = running_thread();     //获取当前正在运行的线程，即获取pcb页的指针
    //当前运行的进程一开始肯定是TASK_RUNNING，但开始执行schedule()时却不一定，见下面thread_block()中的cur->status = status; schedule();
    if(cur->status == TASK_RUNNING){                //若此线程只是cpu时间片到了，将其加入到就绪队列尾
    	ASSERT(!elem_find(&thread_ready_list, &(cur->general_tag)));
    	list_append(&thread_ready_list, &(cur->general_tag));
    	cur->ticks = cur->priority;
    	cur->status = TASK_READY;
    }
    else{
    //此线程需要某事件发生后才能加入就绪队列，加入就绪队列的代码写在处理该事件的函数（本项目是thread_unblock()）里，不写在这里
    }

    //如果就绪队列中没有可运行的任务，就唤醒idle
    if(list_empty(&thread_ready_list))
    	thread_unblock(idle_thread);

    ASSERT(!list_empty(&thread_ready_list));
    thread_tag = NULL;
    thread_tag = list_pop(&thread_ready_list);
    struct task_struct* next = elem2entry(struct task_struct, general_tag, thread_tag);//通过tag求task_struct基址
    next->status = TASK_RUNNING;

    process_activate(next);               //激活任务的页表等
    switch_to(cur, next);
}



void thread_block(enum task_status status){
    ASSERT((status == TASK_BLOCKED) || (status == TASK_WAITING) || (status == TASK_HANGING));
	enum intr_status old = intr_disable();
	struct task_struct* cur = running_thread();                       //获取当前正在运行的线程
	cur->status = status;
	schedule();                                                       //线程阻塞就是使用schedule()调度就绪队列里的其他线程上CPU运行
		                                                              //当然，调用thread_block()前会将该线程加入到某事件的等待队列里
	intr_set_status(old);                                             //待当前线程被解除阻塞后才继续运行这里
}



void thread_unblock(struct task_struct* pthread){
	enum intr_status old = intr_disable();
	ASSERT((pthread->status == TASK_BLOCKED) || (pthread->status == TASK_WAITING) || (pthread->status == TASK_HANGING));
	if(pthread->status != TASK_READY){
		ASSERT(!elem_find(&thread_ready_list, &(pthread->general_tag)));
		if(elem_find(&thread_ready_list, &(pthread->general_tag)))
			PANIC("thread_unblock: blocked thread in ready_list\n");
		list_push(&thread_ready_list, &(pthread->general_tag)); //放到就绪队列最前面，使其尽快得到调度
		pthread->status = TASK_READY;
	}
	intr_set_status(old);
}



//主动让出cpu，换其它线程运行
void thread_yield(void){
    struct task_struct* cur = running_thread();
    enum intr_status old_status = intr_disable();
    ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
    list_append(&thread_ready_list, &cur->general_tag);
    cur->status = TASK_READY;
    schedule();
    intr_set_status(old_status);
}




//以填充空格的方式输出buf
static void pad_print(char* buf, int32_t buf_len, void* ptr, char format){
    memset(buf, 0, buf_len);
    uint8_t out_pad_0idx = 0;
    switch(format){
        case 's':
            out_pad_0idx = sprintf(buf, "%s", ptr);
            break;
        case 'd':
            out_pad_0idx = sprintf(buf, "%d", *((int16_t*)ptr));
            break;
        case 'x':
            out_pad_0idx = sprintf(buf, "%x", *((uint32_t*)ptr));
            break;
    }
    while(out_pad_0idx < buf_len){ // 以空格填充
        buf[out_pad_0idx] = ' ';
        out_pad_0idx++;
    }
    sys_write(stdout_no, buf, buf_len - 1);
}

//用于在list_traversal函数中的回调函数，用于针对线程队列的处理
static bool elem2thread_info(struct list_elem* pelem, int arg UNUSED){
    struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, pelem);
    char out_pad[16] = {0};

    pad_print(out_pad, 16, &pthread->pid, 'd');

    if(pthread->parent_pid == -1){
        pad_print(out_pad, 16, "NULL", 's');
    }
    else{
        pad_print(out_pad, 16, &pthread->parent_pid, 'd');
    }

    switch(pthread->status){
        case 0:
            pad_print(out_pad, 16, "RUNNING", 's');
            break;
        case 1:
            pad_print(out_pad, 16, "READY", 's');
            break;
        case 2:
            pad_print(out_pad, 16, "BLOCKED", 's');
            break;
        case 3:
            pad_print(out_pad, 16, "WAITING", 's');
            break;
        case 4:
            pad_print(out_pad, 16, "HANGING", 's');
            break;
        case 5:
            pad_print(out_pad, 16, "DIED", 's');
    }
    pad_print(out_pad, 16, &pthread->elapsed_ticks, 'x');

    memset(out_pad, 0, 16);
    ASSERT(strlen(pthread->name) < 17);
    memcpy(out_pad, pthread->name, strlen(pthread->name));
    strcat(out_pad, "\n");
    sys_write(stdout_no, out_pad, strlen(out_pad));
    return false;    // 此处返回false是为了迎合主调函数list_traversal，只有回调函数返回false时才会继续调用此函数
}

//打印任务列表
void sys_ps(void){
    char* ps_title = "PID            PPID           STAT           TICKS          COMMAND\n";
    sys_write(stdout_no, ps_title, strlen(ps_title));
    list_traversal(&thread_all_list, elem2thread_info, 0);
}




//回收thread_over的pcb和页表，并将其从调度队列中去除
void thread_exit(struct task_struct* thread_over, bool need_schedule){
    /* 要保证schedule在关中断情况下调用 */
    intr_disable();
    thread_over->status = TASK_DIED;

    /* 如果thread_over不是当前线程，就有可能还在就绪队列中，将其从中删除 */
    if(elem_find(&thread_ready_list, &thread_over->general_tag)){
        list_remove(&thread_over->general_tag);
    }
    if(thread_over->pgdir){     // 如是进程，回收进程的页表
        mfree_page(PF_KERNEL, thread_over->pgdir, 1);
    }

    /* 从all_thread_list中去掉此任务 */
    list_remove(&thread_over->all_list_tag);

    /* 归还pid。必须在回收pcb之前做，pcb所在的页被回收后其页表项也被去掉了，再访问thread_over->pid会引发缺页异常 */
    release_pid(thread_over->pid);

    /* 回收pcb所在的页，主线程的pcb不在堆中，跨过 */
    if(thread_over != main_thread){
        mfree_page(PF_KERNEL, thread_over, 1);
    }

    /* 如果需要下一轮调度则主动调用schedule */
    if(need_schedule){
        schedule();
        PANIC("thread_exit: should not be here\n");
    }
}




//比对任务的pid
static bool pid_check(struct list_elem* pelem, int32_t pid){
    struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, pelem);
    if(pthread->pid == pid){
        return true;
    }
    return false;
}

//根据pid找pcb，若找到则返回该pcb，否则返回NULL
struct task_struct* pid2thread(int32_t pid){
    struct list_elem* pelem = list_traversal(&thread_all_list, pid_check, pid);
    if(pelem == NULL){
        return NULL;
    }
    struct task_struct* thread = elem2entry(struct task_struct, all_list_tag, pelem);
    return thread;
}




void thread_init(){
    put_str("thread_init start!\n");
    list_init(&thread_all_list);
    list_init(&thread_ready_list);
    pid_pool_init();

#if TEST_SECTION == 0                //做原书各节的实验时（sh run.sh 节号，见main.c）不创建init进程，以免它fork出的shell抢键盘输入、打印提示符
    process_execute(init, "init");   //先创建第一个用户进程init，这是第一个进程，故init进程的pid为1
#endif

    make_main_thread();              //将当前main函数创建为线程

    idle_thread = thread_start("idle", 10, idle, NULL);   //创建idle线程
    put_str("thread_init done!\n");
}
