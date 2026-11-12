#!/bin/sh


#时刻注意内核大小（含.bss）有没有超过128KB！loader.asm一次从硬盘读取256个扇区（即128KB）的内核到内存


#用法：sh run.sh         运行最终的shell
#      sh run.sh 9.4     运行原书第9.4节的实验（有哪些实验见kernel/main.c的末尾）
TEST_SECTION=$(echo "${1:-0}" | awk -F. '{print $1*100+$2}')   #把节号9.4转换成904，传给main.c和thread.c

#gcc 15开始默认使用C23标准，bool、true、false成了关键字，与stdint.h中的定义冲突，所以指定使用gnu17标准
CFLAGS="-m32 -std=gnu17 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector -fno-pic -I ./include -DTEST_SECTION=$TEST_SECTION"


echo "制作虚拟硬盘"
qemu-img create -f raw 60M.img 60M      #60M.img用于存放MBR、loader、内核和用户程序cat，每次运行都重新制作
mkdir -p build                          #80M.img是已经分好区的硬盘，用于文件系统，其中的文件会一直保留


echo "编译"
nasm -f bin -o build/mbr.bin  boot/mbr.asm
nasm -f bin -o build/loader.bin  boot/loader.asm

nasm -f elf32 -o build/putchar.o kernel/lib/putchar.asm
nasm -f elf32 -o build/intrrpt.o kernel/lib/interrupt.asm
nasm -f elf32 -o build/switchTo.o kernel/lib/switchTo.asm

gcc $CFLAGS -c -o build/init.o  kernel/init.c
gcc $CFLAGS -c -o build/interrupt.o  kernel/interrupt.c
gcc $CFLAGS -c -o build/print.o  kernel/print.c
gcc $CFLAGS -c -o build/debug.o  kernel/lib/debug.c
gcc $CFLAGS -c -o build/list.o  kernel/lib/list.c
gcc $CFLAGS -c -o build/string.o  kernel/lib/string.c
gcc $CFLAGS -c -o build/bitmap.o  kernel/lib/bitmap.c
gcc $CFLAGS -c -o build/stdio.o  kernel/lib/stdio.c
gcc $CFLAGS -c -o build/stdio-kernel.o  kernel/lib/stdio-kernel.c
gcc $CFLAGS -c -o build/syscall.o  kernel/lib/syscall.c
gcc $CFLAGS -c -o build/assert.o  kernel/lib/assert.c
gcc $CFLAGS -c -o build/memory.o  kernel/memory.c
gcc $CFLAGS -c -o build/thread.o  kernel/thread.c
gcc $CFLAGS -c -o build/timer.o  kernel/timer.c
gcc $CFLAGS -c -o build/sync.o  kernel/sync.c
gcc $CFLAGS -c -o build/ioqueue.o  kernel/ioqueue.c
gcc $CFLAGS -c -o build/keyboard.o  kernel/keyboard.c
gcc $CFLAGS -c -o build/tss.o  kernel/tss.c
gcc $CFLAGS -c -o build/process.o  kernel/process.c
gcc $CFLAGS -c -o build/syscall-init.o  kernel/syscall-init.c
gcc $CFLAGS -c -o build/ide.o  kernel/ide.c
gcc $CFLAGS -c -o build/fs.o  kernel/fs.c
gcc $CFLAGS -c -o build/inode.o  kernel/inode.c
gcc $CFLAGS -c -o build/file.o  kernel/file.c
gcc $CFLAGS -c -o build/dir.o  kernel/dir.c
gcc $CFLAGS -c -o build/fork.o  kernel/fork.c
gcc $CFLAGS -c -o build/exec.o  kernel/exec.c
gcc $CFLAGS -c -o build/wait_exit.o  kernel/wait_exit.c
gcc $CFLAGS -c -o build/pipe.o  kernel/pipe.c
gcc $CFLAGS -c -o build/shell.o  kernel/shell.c
gcc $CFLAGS -c -o build/buildin_cmd.o  kernel/buildin_cmd.c


echo "编译用户程序cat"
nasm -f elf32 -o build/start.o command/start.asm
gcc $CFLAGS -c -o build/cat.o  command/cat.c
ld -m elf_i386  -z noexecstack  -o build/cat  build/start.o build/cat.o build/string.o build/syscall.o build/stdio.o build/assert.o
CAT_SIZE=$(wc -c < build/cat)           #cat的文件大小，内核要用它把cat从硬盘的裸扇区写入文件系统，见main.c
gcc $CFLAGS -DCAT_SIZE=$CAT_SIZE -c -o build/main.o  kernel/main.c




echo "链接"
ld -m elf_i386  -Ttext 0xc0001500 -e main  -z noexecstack   -o build/kernel.elf  \
   build/main.o build/putchar.o build/intrrpt.o build/switchTo.o build/init.o build/interrupt.o build/print.o build/debug.o build/list.o  \
   build/string.o build/bitmap.o build/stdio.o build/stdio-kernel.o build/syscall.o build/assert.o build/memory.o build/thread.o build/timer.o \
   build/sync.o build/ioqueue.o build/keyboard.o build/tss.o build/process.o build/syscall-init.o build/ide.o build/fs.o build/inode.o \
   build/file.o build/dir.o build/fork.o build/exec.o build/wait_exit.o build/pipe.o build/shell.o build/buildin_cmd.o



rm -f  build/*.o



echo "去除ELF文件头"
objcopy -O binary build/kernel.elf  build/kernel.bin
#ld -m elf_i386  -Ttext 0xc0001500 -e main  --oformat binary  -o build/kernel.bin  build/kernel.elf         #改用ld也能运行，但网上说更推荐使用objcopy



echo "烧录"
dd if=build/mbr.bin    of=60M.img  bs=512 count=1 conv=notrunc
dd if=build/loader.bin of=60M.img  bs=512 count=4 seek=2 conv=notrunc
dd if=build/kernel.bin of=60M.img  bs=512 count=256 seek=9 conv=notrunc
dd if=build/cat        of=60M.img  bs=512 seek=300 conv=notrunc    #用户程序cat放在第300个扇区，见main.c

echo "OS启动！"
qemu-system-i386 -m 32  -display curses \
                 -drive file=60M.img,format=raw,if=ide,index=0,media=disk \
                 -drive file=80M.img,format=raw,if=ide,index=1,media=disk
#-m 32  表示分配32MB内存
#-display curses  表示关掉图形界面，直接在当前终端里以文本方式显示（终端窗口至少80列25行）
#退出qemu：按Alt+2进入qemu监视器，输入quit后回车（按Alt+1可从监视器切回OS的界面）
