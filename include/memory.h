#pragma once
#include "stdint.h"
#include "bitmap.h"
#include "list.h"
//前言：
//MBR被BIOS加载到物理地址0x7c00，本项目的loader被MBR加载到物理地址0x900（位于0x500～0x7c00这一片30KB可用区域，放不下我们的内核elf（70KB左右））
//本项目的内核elf被loader加载到物理地址0x70000（位于0x7e00～0x9fc00这一片608KB可用区域），两片可用区域之间隔着512字节的MBR
//本项目的内核映像的main入口物理地址为0x1500，内核映像可以向上覆盖掉MBR（只要不超过0x70000覆盖内核elf就行）
//页目录表和页表放在低端物理内存0～1MB以外，即从0x100000开始。而位图放在低端1MB内存以内，就不用额外考虑它所占用的内存，因为低端1MB内存肯定是被占用的
#define MEM_BITMAP_BASE 0Xc009a000   //内核位图起始虚拟地址，对应的物理地址为0x9a000，减去0x70000得到168KB，在这之间的区域完全足够放下我们的内核elf
                                     //内核初始栈顶虚拟地址为0xc009f000，物理地址0x9f000，是最靠近可用区域上限0x9fc00的，见boot/loader.asm
                                     //将来会在0x9e000处安排内核主进程的PCB或TCB（本项目中PCB或TCB只占用一个自然页），0x9e000到0x9f000刚好是4KB
                                     //主进程（线程）的PCB放在物理低端1MB以内，而其他进程或线程的PCB或TCB创建后放在物理低端1MB以外的广阔空间
                                     //主进程（线程）的初始栈顶0x9f000（虚拟地址0xc009f000）是本项目在低端1MB所用到的最高物理地址


#define K_HEAP_START    0xc0100000   //目前内核可用的堆空间的起始虚拟地址：0xc0100000～0xffbfffff ————从0xffc00000开始的空间都被第1023个PDE浪费了
                                     //目前用户可用的堆空间的虚拟地址为：    0x00100000～0xbfffffff ————这是对4GB内存而言
/*之前已完成的虚拟地址到物理地址的映射：详见loader.asm
0x00000000-0x000fffff -> 0x000000000000-0x0000000fffff //第0个PDE指向第0个页表，而第0个页表中（共1024个PTE）的256个PTE分配给了物理低端1MB
0xc0000000-0xc00fffff -> 0x000000000000-0x0000000fffff //第768个PDE指向第0个页表，而第0个页表中（共1024个PTE）的256个PTE分配给了物理低端1MB
0xffc00000-0xffc00fff -> 0x000000101000-0x000000101fff //第1023个PDE指向页目录表自身（0x100000），被当成页表，利用虚拟地址中间10位索引到第0个PTE
0xfff00000-0xffffefff -> 0x000000101000-0x0000001fffff //第1023个PDE指向页目录表自身，被当成页表，利用虚拟地址中间10位索引到第768～1022共255个PTE
0xfffff000-0xffffffff -> 0x000000100000-0x000000100fff //第1023个PDE指向页目录表自身（0x100000），被当成页表，利用虚拟地址中间10位索引到第1023个PTE
*/


#define PG_P_1 1     //此页已存在于内存
#define PG_P_0 0     //此页不存在于内存，在本项目中不使用
#define PG_RW_R 0    //读、执行
#define PG_RW_W 2    //读、写、执行
#define PG_US_S 0    //系统级
#define PG_US_U 4    //用户级


enum pool_flags{
    PF_KERNEL = 1, //内核内存池
    PF_USER = 2    //用户内存池
};


struct virtual_addr{
    struct bitmap vaddr_bitmap;     //虚拟内存池用到的位图
    unsigned int vaddr_start;       //虚拟内存池的起始虚拟地址
};


/* 内存块 */
struct mem_block{
    struct list_elem free_elem;
};

/* 内存块描述符 */
struct mem_block_desc{
    uint32_t block_size;            //内存块大小
    uint32_t blocks_per_arena;      //本arena中可容纳此mem_block的数量
    struct list free_list;          //目前可用的mem_block链表
};

#define DESC_CNT 7                  //内存块描述符的个数：16、32、64、128、256、512、1024字节共7种规格


extern struct pool kernel_pool,user_pool;

void mem_pool_init(uint32_t all_mem);
void mem_init();                                           //初始化内存池：对kernel_vaddr, kernel_pool, user_pool这3个结构进行初始化


void* vaddr_get(enum pool_flags pf,uint32_t pg_cnt);
uint32_t* pde_ptr(uint32_t vaddr);
uint32_t* pte_ptr(uint32_t vaddr);
void* palloc(struct pool* m_pool);
void page_table_add(void* _vaddr,void* _page_phyaddr);

void* malloc_page(enum pool_flags pf,uint32_t pg_cnt);     //分配pg_cnt个页（物理页可以不连续，虚拟页要连续），成功则返回起始虚拟地址，失败返回NULL
void* get_kernel_pages(uint32_t pg_cnt);                   //malloc_page(PF_KERNEL, pg_cnt)并清零虚拟页（对应的物理页也会清零）
void* get_user_pages(uint32_t pg_cnt);                     //在用户空间中申请pg_cnt页内存并清零
void* get_a_page(enum pool_flags pf, uint32_t vaddr);      //将虚拟地址vaddr与pf池中的物理地址关联，仅支持一页
void* get_a_page_without_opvaddrbitmap(enum pool_flags pf, uint32_t vaddr); //同上，但不操作虚拟地址位图，fork时使用
uint32_t addr_v2p(uint32_t vaddr);                         //得到虚拟地址vaddr映射到的物理地址

void block_desc_init(struct mem_block_desc* desc_array);   //初始化内存块描述符数组，为malloc做准备
void* sys_malloc(uint32_t size);                           //在堆中申请size字节内存
void sys_free(void* ptr);                                  //回收sys_malloc申请的内存ptr
void pfree(uint32_t pg_phy_addr);                          //将物理页pg_phy_addr回收到物理内存池
void mfree_page(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt); //释放以虚拟地址_vaddr起始的pg_cnt个页
void free_a_phy_page(uint32_t pg_phy_addr);                //只在物理内存池的位图中回收物理页，不改动页表
