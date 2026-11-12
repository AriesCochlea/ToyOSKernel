//我们的入口函数的虚拟地址被固定为0xc0001500，故main函数前面不能有任何实质性代码段



#include "stdint.h"
#include "print.h"
#include "init.h"
#include "thread.h"
#include "global.h"
#include "memory.h"
#include "ide.h"
#include "fs.h"
#include "stdio-kernel.h"
#include "syscall.h"
#include "stdio.h"
#include "assert.h"
#include "shell.h"
#include "debug.h"
#include "interrupt.h"
#include "keyboard.h"
#include "process.h"
#include "syscall-init.h"
#include "dir.h"
#include "string.h"
#include "timer.h"


#define CAT_START_SECTOR 300   //用户程序cat在硬盘60M.img上的起始扇区，见run.sh
                               //cat的文件大小CAT_SIZE（字节）由run.sh在编译本文件时通过gcc的-D选项传入

void init(void);
void test_section(void);       //原书各节的实验，见本文件末尾。只有用sh run.sh 节号运行时才会调用


int main() {

	init_all();


#if TEST_SECTION == 0    //sh run.sh：运行最终的shell

    /*************    将用户程序cat写入文件系统（文件系统中已有/cat时会创建失败，直接跳过）    *************/
    uint32_t sec_cnt = DIV_ROUND_UP(CAT_SIZE, 512);
    struct disk* sda = &channels[0].devices[0];
    void* prog_buf = sys_malloc(sec_cnt * 512);
    ide_read(sda, CAT_START_SECTOR, prog_buf, sec_cnt);
    int32_t fd = sys_open("/cat", O_CREAT|O_RDWR);
    if (fd != -1) {
        if (sys_write(fd, prog_buf, CAT_SIZE) == -1) {
            printk("file write error!\n");
            while(1);
        }
        sys_close(fd);
    }
    sys_free(prog_buf);
    /*************    写入应用程序结束   *************/


    cls_screen();
    put_str("[rabbit@localhost /]$ ");   //init进程fork出的shell已经打印过提示符，但被上面的cls_screen()清掉了，所以再打印一次

#else                    //sh run.sh 节号：运行原书该节的实验

    cls_screen();                        //清掉初始化时打印的信息，只显示实验的输出
    printk("section %d.%d test:\n", TEST_SECTION / 100, TEST_SECTION % 100);
    test_section();

#endif

    thread_exit(running_thread(), true); //主线程的任务完成，退出（实验中创建的线程和进程会继续运行）
    return 0;
}



/* init进程：第一个用户进程，pid为1 */
void init(void) {
    uint32_t ret_pid = fork();
    if(ret_pid) {  // 父进程
        int status;
        int child_pid;
        /* init在此处不停的回收僵尸进程 */
        while(1) {
            child_pid = wait(&status);
            printf("I`m init, My pid is 1, I recieve a child, It`s pid is %d, status is %d\n", child_pid, status);
        }
    } else {	  // 子进程
        my_shell();
    }
    panic("init: should not be here");
}




/***************************************  原书各节的实验  ***************************************
 * 用法：sh run.sh 节号，如sh run.sh 9.4。每章挑了1～2个有代表性的实验：
 *   6.3          实现自己的打印函数
 *   7.6          编写中断处理程序：时钟中断计数、除0异常
 *   8.2          实现assert断言
 *   8.5          内存管理系统：分配页内存
 *   9.4          多线程调度
 *   10.5         环形输入缓冲区：两个消费者线程读取键盘输入
 *   11.3         实现用户进程
 *   12.2～12.3   系统调用getpid，让用户进程“说话”（printf）
 *   12.4         完善堆内存管理：malloc和free
 *   13.2         编写硬盘驱动程序：分区信息、读写扇区
 *   14.2         创建文件系统：打印挂载分区的超级块
 *   14.5～14.10  文件的创建、打开与关闭、写入、读取、读写指针定位、删除
 *   14.11～14.15 目录的创建、遍历、删除，任务的工作目录，获得文件属性
 *   15.1         fork
 *   15.2～15.3   read、putchar、clear系统调用：回显键盘输入，ctrl+l清屏
 *   15.4         简单的shell
 *   15.5         exec加载硬盘上的用户程序cat，并传入参数
 *   15.6～15.7   wait和exit，利用管道实现进程间通信
 * 注意：
 *   1.做实验时不创建init进程和shell（见thread.c的thread_init），所以第15章的实验由main创建的用户进程来做
 *   2.内核线程用put_str、printk等函数打印；用户进程只能通过系统调用printf等打印，put_str会关中断，在3特权级执行会引发异常
 *   3.线程和用户进程的函数不能返回，最后要while(1)
 *   4.屏幕是VGA文本模式，只能显示英文字符
 *   5.第14章的实验会读写80M.img上的文件系统，实验结束前会删除创建的文件和目录
 ************************************************************************************************/

#if TEST_SECTION == 0

#elif TEST_SECTION == 603        //6.3 实现自己的打印函数
void test_section(void) {
    cls_screen();        //清屏，使光标位于屏幕左上角
    put_char('\b');      //测试屏幕左上角的'\b'不退格
    put_str("Hee");
    put_char('\b');      //测试'\b'退格
    put_char('\0');      //测试空字符'\0'不显示
    put_str("llo OS\n");
    put_str("version 1.0 : ");
    put_uint(2022);      //无符号整数
    put_int(-7);         //有符号整数
    put_int(-1);
    put_char('\n');
}

#elif TEST_SECTION == 706        //7.6 编写中断处理程序
extern unsigned int total_ticks; //时钟中断发生的总次数，见timer.c
void test_section(void) {
    int i;
    for (i = 0; i < 5; i++) {    //每隔1秒打印一次时钟中断发生的次数，可以看到它在不断增长
        printk("total_ticks: %d\n", total_ticks);
        mtime_sleep(1000);
    }
    printk("divide by zero now:\n");
    volatile int zero = 0;
    int a = 1 / zero;            //除0会引发0号异常#DE，由interrupt.c中的通用中断处理函数打印异常信息并停机
    printk("should not be here %d\n", a);
}

#elif TEST_SECTION == 802        //8.2 实现assert断言
void test_section(void) {
    printk("ASSERT(1 == 2) now:\n");
    ASSERT(1 == 2);              //断言失败，打印出错的文件名、行号、函数名和条件后停机
}

#elif TEST_SECTION == 805        //8.5 内存管理系统：分配页内存
void test_section(void) {
    void* addr = get_kernel_pages(3);   //从内核内存池中申请3页内存
    printk("get_kernel_pages start vaddr is 0x%x\n", (uint32_t)addr);
    int i;
    for (i = 0; i < 3; i++) {           //虚拟地址是连续的，物理地址不一定连续
        uint32_t vaddr = (uint32_t)addr + i * PG_SIZE;
        printk("   page %d: vaddr 0x%x -> paddr 0x%x\n", i, vaddr, addr_v2p(vaddr));
    }
}

#elif TEST_SECTION == 904        //9.4 多线程调度
static void k_thread(void* arg) {
    while(1)
        put_str((char*)arg);
}
void test_section(void) {
    thread_start("k_thread_a", 31, k_thread, "argA ");   //优先级31，即每次上cpu后运行31个时钟中断的时间
    thread_start("k_thread_b", 8, k_thread, "argB ");    //优先级8，打印的数量大约是k_thread_a的1/4
    while(1)
        put_str("Main ");                                  //主线程的优先级是32，见thread.c
}

#elif TEST_SECTION == 1005       //10.5 环形输入缓冲区：生产者与消费者实例测试
static void consumer(void* arg) {
    while(1) {
        enum intr_status old_status = intr_disable();
        if (ioq_length(&kbd_buf) > 0) {   //原书用的是ioq_empty，它在本项目的ioqueue.c中是静态函数
            put_str((char*)arg);
            char byte = ioq_getchar(&kbd_buf);
            put_char(byte);
        }
        intr_set_status(old_status);
    }
}
void test_section(void) {
    printk("press any key, consumer_a prints A_x and consumer_b prints B_x\n");
    thread_start("consumer_a", 31, consumer, " A_");      //键盘中断处理程序是生产者，这两个线程是消费者
    thread_start("consumer_b", 31, consumer, " B_");
}

#elif TEST_SECTION == 1103       //11.3 实现用户进程
int test_var_a = 0, test_var_b = 0;
static void k_thread_a(void* arg UNUSED) {     //内核线程打印用户进程修改的全局变量
    while(1) {
        put_str(" v_a:0x");
        put_16uint(test_var_a);
    }
}
static void k_thread_b(void* arg UNUSED) {
    while(1) {
        put_str(" v_b:0x");
        put_16uint(test_var_b);
    }
}
static void u_prog_a(void) {                   //用户进程运行在3特权级，不能直接打印，只修改全局变量
    while(1)
        test_var_a++;
}
static void u_prog_b(void) {
    while(1)
        test_var_b++;
}
void test_section(void) {
    thread_start("k_thread_a", 31, k_thread_a, NULL);
    thread_start("k_thread_b", 31, k_thread_b, NULL);
    process_execute(u_prog_a, "user_prog_a");
    process_execute(u_prog_b, "user_prog_b");
}

#elif TEST_SECTION >= 1202 && TEST_SECTION <= 1203   //12.2 系统调用getpid，12.3 让用户进程“说话”
static void k_thread(void* arg) {              //内核线程直接调用sys_getpid
    printk(" I am %s, my pid:%d\n", (char*)arg, sys_getpid());
    while(1);
}
static void u_prog_a(void) {                   //用户进程通过系统调用getpid和printf
    printf(" I am %s, my pid:%d%c", "prog_a", getpid(), '\n');
    while(1);
}
static void u_prog_b(void) {
    printf(" I am %s, my pid:%d%c", "prog_b", getpid(), '\n');
    while(1);
}
void test_section(void) {
    printk(" I am main, my pid:%d\n", sys_getpid());
    process_execute(u_prog_a, "user_prog_a");
    process_execute(u_prog_b, "user_prog_b");
    thread_start("k_thread_a", 31, k_thread, "thread_a");
    thread_start("k_thread_b", 31, k_thread, "thread_b");
}

#elif TEST_SECTION == 1204       //12.4 完善堆内存管理：malloc和free
static void k_thread(void* arg) {              //内核线程调用sys_malloc和sys_free，在内核的堆中分配内存
    void* addr1 = sys_malloc(256);
    void* addr2 = sys_malloc(255);
    void* addr3 = sys_malloc(254);
    printk(" %s malloc addr:0x%x,0x%x,0x%x\n", (char*)arg, (int)addr1, (int)addr2, (int)addr3);
    sys_free(addr1);
    sys_free(addr2);
    sys_free(addr3);
    while(1);
}
static void u_prog_a(void) {                   //用户进程通过系统调用malloc和free，在自己的堆中分配内存
    void* addr1 = malloc(256);
    void* addr2 = malloc(255);
    void* addr3 = malloc(254);
    printf(" prog_a malloc addr:0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2, (int)addr3);
    free(addr1);
    free(addr2);
    free(addr3);
    while(1);
}
static void u_prog_b(void) {
    void* addr1 = malloc(256);
    void* addr2 = malloc(255);
    void* addr3 = malloc(254);
    printf(" prog_b malloc addr:0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2, (int)addr3);
    free(addr1);
    free(addr2);
    free(addr3);
    while(1);
}
void test_section(void) {
    process_execute(u_prog_a, "u_prog_a");
    process_execute(u_prog_b, "u_prog_b");
    thread_start("k_thread_a", 31, k_thread, "thread_a");
    thread_start("k_thread_b", 31, k_thread, "thread_b");
}

#elif TEST_SECTION == 1302       //13.2 编写硬盘驱动程序
static bool print_partition(struct list_elem* pelem, int arg UNUSED) {
    struct partition* part = elem2entry(struct partition, part_tag, pelem);
    printk("   %s start_lba:0x%x, sec_cnt:0x%x\n", part->name, part->start_lba, part->sec_cnt);
    return false;                //返回false使list_traversal继续遍历
}
void test_section(void) {
    printk("all partitions of disk sdb (80M.img):\n");
    list_traversal(&partition_list, print_partition, 0);

    /* 读写sda（60M.img）上空闲的第500个扇区。每次运行run.sh都会重新创建60M.img，不会影响别的数据 */
    struct disk* sda = &channels[0].devices[0];
    char* buf = sys_malloc(512);
    memset(buf, 0, 512);
    strcpy(buf, "hello, disk!");
    ide_write(sda, 500, buf, 1);
    memset(buf, 0, 512);
    ide_read(sda, 500, buf, 1);
    printk("write \"hello, disk!\" to sector 500 of sda, read back: \"%s\"\n", buf);
    sys_free(buf);
}

#elif TEST_SECTION == 1402       //14.2 创建文件系统：init_all中的filesys_init已格式化（首次）并挂载了sdb1，这里打印其超级块
#include "super_block.h"
static bool print_fs(struct list_elem* pelem, int arg) {   //同fs.c中filesys_init：读出各分区的超级块，根据魔数判断是否有文件系统
    struct partition* part = elem2entry(struct partition, part_tag, pelem);
    struct super_block* sb_buf = (struct super_block*)arg;
    memset(sb_buf, 0, SECTOR_SIZE);
    ide_read(part->my_disk, part->start_lba + 1, sb_buf, 1);
    printk(sb_buf->magic == 0x19590318 ? "%s has filesystem\n" : "%s has no filesystem\n", part->name);
    return false;
}
void test_section(void) {
    printk("searching filesystem......\n");
    struct super_block* sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);
    list_traversal(&partition_list, print_fs, (int)sb_buf);
    sys_free(sb_buf);

    struct super_block* sb = cur_part->sb;   //打印格式同fs.c中partition_format（即原书14.2.2）
    printk("%s info:\n", cur_part->name);
    printk("   magic:0x%x\n   part_lba_base:0x%x\n   all_sectors:0x%x\n   inode_cnt:0x%x\n   block_bitmap_lba:0x%x\n   block_bitmap_sectors:0x%x\n   inode_bitmap_lba:0x%x\n   inode_bitmap_sectors:0x%x\n   inode_table_lba:0x%x\n   inode_table_sectors:0x%x\n   data_start_lba:0x%x\n", sb->magic, sb->part_lba_base, sb->sec_cnt, sb->inode_cnt, sb->block_bitmap_lba, sb->block_bitmap_sects, sb->inode_bitmap_lba, sb->inode_bitmap_sects, sb->inode_table_lba, sb->inode_table_sects, sb->data_start_lba);
    printk("   super_block_lba:0x%x\n", cur_part->start_lba + 1);
    printk("   root_dir_lba:0x%x\n", sb->data_start_lba);
    printk("mount %s done!\n", cur_part->name);
}

#elif TEST_SECTION >= 1405 && TEST_SECTION <= 1410   //14.5～14.10 文件的创建、打开与关闭、写入、读取、读写指针定位、删除
void test_section(void) {
    int32_t fd = sys_open("/file1", O_CREAT | O_RDWR);          //14.5 创建文件
    if (fd == -1) {                      //上次实验中途退出时可能留下了/file1，删除后重新创建
        sys_unlink("/file1");
        fd = sys_open("/file1", O_CREAT | O_RDWR);
    }
    printk("create /file1, fd:%d\n", fd);
    printk("write %d bytes to /file1\n", sys_write(fd, "hello,world\n", 12));   //14.7 写入文件
    sys_close(fd);                                                //14.6 关闭文件

    fd = sys_open("/file1", O_RDWR);                             //14.6 打开文件
    printk("open /file1, fd:%d\n", fd);
    char buf[32] = {0};
    int32_t read_bytes = sys_read(fd, buf, 5);                    //14.8 读取文件
    printk("read %d bytes: %s\n", read_bytes, buf);
    memset(buf, 0, 32);
    read_bytes = sys_read(fd, buf, 32);
    printk("read %d bytes: %s", read_bytes, buf);
    sys_lseek(fd, 0, SEEK_SET);                                   //14.9 把文件读写指针定位到文件开头
    memset(buf, 0, 32);
    read_bytes = sys_read(fd, buf, 32);
    printk("lseek to 0, read %d bytes: %s", read_bytes, buf);
    sys_close(fd);

    printk("unlink /file1 %s\n", sys_unlink("/file1") == 0 ? "done" : "failed");   //14.10 删除文件
}

#elif TEST_SECTION >= 1411 && TEST_SECTION <= 1415   //14.11～14.15 目录的创建、遍历、删除，任务的工作目录，获得文件属性
void test_section(void) {
    printk("mkdir /dir1 %s\n", sys_mkdir("/dir1") == 0 ? "done" : "failed");                    //14.11 创建目录
    printk("mkdir /dir1/subdir1 %s\n", sys_mkdir("/dir1/subdir1") == 0 ? "done" : "failed");
    int32_t fd = sys_open("/dir1/file2", O_CREAT | O_RDWR);                                      //在目录中创建一个文件
    if (fd != -1) {
        sys_write(fd, "hello\n", 6);
        sys_close(fd);
    }

    struct dir* dir = sys_opendir("/dir1");                                                      //14.12 遍历目录
    if (dir != NULL) {
        printk("content of /dir1:\n");
        struct dir_entry* dir_e;
        while ((dir_e = sys_readdir(dir)) != NULL) {
            printk("   %s  %s\n", dir_e->f_type == FT_DIRECTORY ? "directory" : "regular  ", dir_e->filename);
        }
        sys_closedir(dir);
    }

    char cwd[MAX_PATH_LEN] = {0};                                                                //14.14 任务的工作目录
    sys_chdir("/dir1/subdir1");
    printk("chdir to /dir1/subdir1, cwd: %s\n", sys_getcwd(cwd, MAX_PATH_LEN));
    sys_chdir("/");
    printk("chdir to /, cwd: %s\n", sys_getcwd(cwd, MAX_PATH_LEN));

    struct stat st;                                                                             //14.15 获得文件属性
    if (sys_stat("/dir1/file2", &st) == 0) {
        printk("stat /dir1/file2: inode %d, size %d, %s\n", st.st_ino, st.st_size, st.st_filetype == FT_DIRECTORY ? "directory" : "regular");
    }

    /* 14.13 删除目录：非空目录不能删除，要先删除目录中的文件和子目录 */
    printk("rmdir /dir1 (not empty) %s\n", sys_rmdir("/dir1") == 0 ? "done" : "failed");
    sys_unlink("/dir1/file2");
    sys_rmdir("/dir1/subdir1");
    printk("rmdir /dir1 (empty) %s\n", sys_rmdir("/dir1") == 0 ? "done" : "failed");
}

#elif TEST_SECTION == 1501       //15.1 fork的原理与实现
static void u_fork_test(void) {               //fork只能在用户进程中调用
    printf("pid_t pid = fork();\n");
    pid_t pid = fork();
    if (pid) {
        printf("i am father, my pid is %d, child pid is %d\n", getpid(), pid);
    } else {
        printf("i am child, my pid is %d, ret pid is %d\n", getpid(), pid);
    }
    while(1);
}
void test_section(void) {
    process_execute(u_fork_test, "fork_test");
}

#elif TEST_SECTION >= 1502 && TEST_SECTION <= 1503   //15.2 添加read系统调用，获取键盘输入，15.3 添加putchar、clear系统调用
static void u_read_test(void) {
    printf("press any key to echo, ctrl+l to clear screen\n");
    char c;
    while (1) {
        read(0, &c, 1);          //从标准输入（键盘）读取1个字符，没有输入时阻塞
        if (c == 'l' - 'a') {    //ctrl+l
            clear();
        } else {
            putchar(c);
        }
    }
}
void test_section(void) {
    process_execute(u_read_test, "read_test");
}

#elif TEST_SECTION == 1504       //15.4 实现一个简单的shell
void test_section(void) {
    process_execute(my_shell, "shell");   //实验时没有init进程，由main直接创建shell进程（外部命令cat不可用，见15.5）
}

#elif TEST_SECTION == 1505       //15.5 加载用户进程：exec执行硬盘上的用户程序cat，并传入参数
static char* cat_argv[] = {"/cat", "/file3", NULL};   //放在内核数据区，execv替换用户进程体后仍可访问
static void u_exec_test(void) {
    pid_t pid = fork();
    if (pid) {
        int32_t status;
        pid_t child_pid = wait(&status);
        printf("I`m father, child %d exit, its status is %d\n", child_pid, status);   //cat的main返回66
        unlink("/file3");
        while(1);
    } else {
        execv(cat_argv[0], cat_argv);
        printf("execv failed\n");
        exit(-1);
    }
}
void test_section(void) {
    /* 将用户程序cat写入文件系统，同TEST_SECTION == 0（文件系统中已有/cat时会创建失败，直接跳过） */
    uint32_t sec_cnt = DIV_ROUND_UP(CAT_SIZE, 512);
    struct disk* sda = &channels[0].devices[0];
    void* prog_buf = sys_malloc(sec_cnt * 512);
    ide_read(sda, CAT_START_SECTOR, prog_buf, sec_cnt);
    int32_t fd = sys_open("/cat", O_CREAT|O_RDWR);
    if (fd != -1) {
        sys_write(fd, prog_buf, CAT_SIZE);
        sys_close(fd);
    }
    sys_free(prog_buf);

    fd = sys_open("/file3", O_CREAT | O_RDWR);   //给cat准备一个文件，cat /file3会打印它的内容
    if (fd == -1) {
        sys_unlink("/file3");
        fd = sys_open("/file3", O_CREAT | O_RDWR);
    }
    sys_write(fd, "hello, I am file3\n", 18);
    sys_close(fd);

    process_execute(u_exec_test, "exec_test");
}

#elif TEST_SECTION >= 1506 && TEST_SECTION <= 1507   //15.6 实现系统调用wait和exit，15.7 利用管道实现进程间通信
static void u_pipe_test(void) {
    int32_t fd[2] = {-1};
    pipe(fd);
    pid_t pid = fork();
    if (pid) {      // 父进程：往管道中写数据，然后等待子进程退出
        close(fd[0]);
        write(fd[1], "Hi, my son, I love you!", 24);
        printf("I`m father, my pid is %d\n", getpid());
        int32_t status;
        pid_t child_pid = wait(&status);
        printf("I`m father, my child %d exit, its status is %d\n", child_pid, status);
        close(fd[1]);
        while(1);
    } else {        // 子进程：从管道中读出数据，然后以9为状态值退出
        close(fd[1]);
        char buf[32] = {0};
        read(fd[0], buf, 24);
        printf("I`m child, my pid is %d\n", getpid());
        printf("I`m child, my father said to me: \"%s\"\n", buf);
        exit(9);
    }
}
void test_section(void) {
    process_execute(u_pipe_test, "pipe_test");
}

#else                            //没有为该节编写实验
void test_section(void) {
    printk("no experiment for this section, see the list at the end of kernel/main.c\n");
}
#endif
