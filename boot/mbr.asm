;PC机加电后，CPU的cs:ip寄存器被硬件强制初始化为0xF000:0xFFF0，即从内存的物理地址0xFFFF0处运行BIOS（被写死到内存的ROM里了），
;BIOS检查并初始化硬件，建立中断向量表并填写中断例程，最后校验启动盘第一个扇区（即MBR）的内容（以魔数0x55, 0xaa结束），并将MBR加载到物理地址0x7c00 
;CHS地址用起来不方便，本项目读写硬盘时采用LBA地址，从第0个扇区开始算起，上不封顶。


LOADER_START_SECTOR equ 0x2    ; loader被本项目放置在硬盘的第2个扇区。注：第0个扇区固定是MBR；每个扇区为512字节。
LOADER_BASE_ADDR equ 0x900     ; MBR将loader从硬盘从读入后放入内存的物理地址0x900处


; 主引导程序开始
;-----------------------------------------------
;如果没有使用伪指令section, 编译器默认整个文件全部代码同为一个section。一般数据放在一个section中，指令放在另一个section中，但不强制。
;伪指令section并没有对程序中的地址产生任何影响，有没有section都一个样， section中的地址依然是相对于起始地址的顺延。指定vstart后才会产生影响。
;“vstart＝Ox7c00”表示本程序在编译时告诉编译器，把本section的起始地址编译为Ox7c00，其他内容的地址是0x7c00+偏移量，这样汇编后就不用链接了，直接就能运行。
;因为编译器没有把程序加载到内存的功能，所以vstart是“虚拟”地址。                            
                            
section MBR vstart=0x7c00   
                            
;初始化实模式下用到的寄存器： ;已有cs==0, ip==0x7c00
    mov ax, cs
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov sp, 0x7c00
    mov ax, 0xb800                 ;显卡的文本模式的物理地址
    mov gs, ax
	mov ax, 0600h 
    mov bx, 0700h
    mov cx, 0
    mov dx, 184fh
    int 0x10                       ;调用BIOS的0x10号中断的0x06号功能来清屏
    
.set_cursor:                       ;将光标坐标置为0（即屏幕左上角），通过读（int）、写（out）数据端口寄存器0x03d5来获得或设置光标位置
    mov dx, 0x03d4
    mov al, 0x0e                   ;用于光标位置的高8位
    out dx, al
    mov dx, 0x03d5
    mov al, 0
    out dx, al

    mov dx, 0x03d4
    mov al, 0x0f                   ;用于光标位置的低8位
    out dx, al
    mov dx, 0x03d5
    mov al, 0
    out dx, al


;-----------------------------------------------------------
; 将硬盘上的loader（操作系统内核加载器）加载到内存
;-----------------------------------------------------------
;可参考：https://blog.csdn.net/cosmoslife/article/details/9164045
;       https://blog.csdn.net/farmwang/article/details/50017225
;       https://www.docin.com/p-1548256149.html
;int 0x13的扩展模式的调用过程：
;  首先在磁盘地址数据结构体packet中设置正确的值
;  设置DS:SI指向数据结构packet在内存中的地址。DS已经在上面初始化为0了，只需要初始化SI即可
;  读出硬盘数据到内存：子功能号AH=0x42。写入内存数据到硬盘：子功能号AH=0x43
;  DL=驱动器编号
;  调用int 0x13
;在读取的过程中出现错误的话，AH=错误码，进位标志CF会被置1。执行成功的话，AH置0，CF也会被置0。           

jmp load_loader

packet:
packet_size: db 0x10                ;可以为0x10或0x18，即16或24字节
reserved:    db 0x00                ;保留项，固定为0
count:       dw 0x0004              ;要读取的扇区数
bufferoff:   dw LOADER_BASE_ADDR    ;缓冲区的偏移地址：0x900
bufferseg:   dw 0                   ;缓冲区的段基址，ds寄存器已经在上面初始化为0了
startsector: dq LOADER_START_SECTOR ;要读取的起始扇区的LBA地址：0x02
;buffaddr:   dq 0                   ;64位平坦模型下的buffer address，在packet_size设置成16字节时是无效的  

load_loader:
	mov ax, 0
	mov ah, 0x42
	mov dl, 0x80                    ;dl为驱动器号0x80～0xff，0x80表示第一块硬盘（U盘）————也可能dl并非从0x80开始连续递增，0x81也并不一定指向从盘
	mov si, packet                  ;DS已经在上面初始化为0了，只需要初始化SI即可
	int 0x13
	
    cmp ah, 0 
    je  LOADER_BASE_ADDR + 0x300    ;直接跳到loader.asm里的loader_start处开始执行
    
	mov byte [gs:0x00],'M'          ;如果硬盘（或U盘）读取失败，就会显示'M'
    jmp $

                                    ;$属于“隐式地”藏在本行代码前的标号，$$指代本section的起始地址，都是编译器给安排的。
    times 446-($-$$) db 0           ;$-$$是本行到本section起始地址的偏移量。

;MBR分区表由4项组成，每项16个字节，共64字节。本项目只为硬盘划分一个1GB的分区，故只需要一个分区表项。这是写给BIOS“看”的，为了能用U盘启动并在物理机上运行（但极不推荐，后果自负）。本项目不会用到：
	db 0x80                 ;若值为0x80表示活动分区，若值为0x00表示非活动分区
    db 0x00,0x03,0x00       ;本分区的起始CHS地址：磁头号（第二字节）、扇区号（第三字节的低6位）、柱面号（第三字节高2位+第四字节）
    db 0x0b                 ;第五字节为分区类型，0x0b代表FAT32文件系统的主分区
    db 0x21,0xff,0xff       ;本分区的结束CHS地址：磁头号（第六字节）、扇区号（第七字节的低6位）、柱面号（第七字节高2位+第八字节）。CHS地址形同虚设。
    db 0x02,0x00,0x00,0x00  ;第9、10、11、12字节为本分区的起始LBA地址，反向(0x00 00 00 02) = 2
    db 0x00,0x00,0x20,0x00  ;第13、14、15、16字节为本分区的总扇区数，反向(0x00 20 00 00) = 2097152

    times 510-($-$$) db 0  ;补满510字节，加上下面2字节的魔数，共512字节
    db 0x55, 0xaa          ;2字节的魔数，标记MBR的结尾