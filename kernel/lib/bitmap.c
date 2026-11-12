#include "bitmap.h"            //函数定义
#include "string.h"            //memset函数要用
#include "interrupt.h"         
#include "print.h"             
#include "debug.h"
#include "stdint.h"





void bitmap_init(struct bitmap* btmp)
{	if(btmp == NULL)
		return;
    memset(btmp->bits,0,btmp->btmp_bytes_len);
}





//判断bit_idx位是否为1，若为1返回true，否则返回false
int bitmap_scan_test(struct bitmap* btmp,uint32_t bit_idx)  //一个8位的数 bit_idx/8 找数组下标 %得索引下的具体位置
{
	if(btmp == NULL || bit_idx >= btmp->btmp_bytes_len*8)
		return 0;
    uint32_t byte_idx = bit_idx/8;
    uint32_t byte_pos = bit_idx%8;
    if (btmp->bits[byte_idx] & (BITMAP_MASK << byte_pos))
    	return 1;
    else 
    	return 0;	
}




//将位图第bit_idx位起连续cnt个位设置为value
void bitmap_set(struct bitmap* btmp,uint32_t bit_idx,int8_t value)
{
    ASSERT(value == 1 || value == 0);
    uint32_t byte_idx = bit_idx/8;
    uint32_t byte_pos = bit_idx%8;
    if(value)	btmp->bits[byte_idx] |=  (BITMAP_MASK << byte_pos);
    else	btmp->bits[byte_idx] &= ~(BITMAP_MASK << byte_pos);
}





/*
   这个函数写很多的原因是因为刚开始先用一个字节的快速扫描
   看是否每个字节中存在位为0的位置 然后紧接着再看连续位 挺有意思 自己先写写看
*/

/* 在位图中申请连续cnt个位，成功则返回其起始位下标，失败则返回-1 */
int bitmap_scan(struct bitmap* btmp,uint32_t cnt){
    uint32_t idx_byte=0;   // 用于记录空闲位所在的字节
    /* 逐字节比较，蛮力法 */
    while ((0xff==btmp->bits[idx_byte]) && (idx_byte<btmp->btmp_bytes_len)){
    // 1表示已分配，若为0xff，说明该字节内已无空闲位，向下一字节继续找
    idx_byte++;
    }

    ASSERT(idx_byte<btmp->btmp_bytes_len);
    if (idx_byte==btmp->btmp_bytes_len) {   // 若内存池中找不到可用空间
        return -1;
    }

    /* 若在位图数组范围内的某字节内找到了空闲位
     * 在该字节内逐位比对，返回空闲位索引 */
    int idx_bit=0;
    /* 和btmp->bits[idx_byte]这个字节逐位比对 */
    while ((uint8_t)(BITMAP_MASK<<idx_bit)&(btmp->bits[idx_byte])){
    idx_bit++;
    }

    int bit_idx_start=idx_byte*8+idx_bit;   // 空闲位在位图内的下标
    if (cnt==1){
        return bit_idx_start;
    }
    uint32_t bit_left=(btmp->btmp_bytes_len*8-bit_idx_start);
    // 记录还有多少位可以判断
    uint32_t next_bit=bit_idx_start+1;
    uint32_t count=1;   // 用于记录找到的空闲位的个数

    bit_idx_start=-1;
    while (bit_left-->0){
    if (!bitmap_scan_test(btmp,next_bit)){
        count++;
    }else{
        count=0;
    }
    if (count==cnt){
        bit_idx_start=next_bit-cnt+1;
        break;
    }
    next_bit++;
    }
    return bit_idx_start;
}

/*
int  bitmap_scan(struct bitmap* btmp, unsigned int cnt){
	if(btmp == NULL || cnt == 0)
		return -1;
	unsigned int max_cnt = btmp->btmp_bytes_len*8;
	if(cnt > max_cnt)
		return -1;
	for(unsigned int i=0; i<= max_cnt -cnt; ++i){
		unsigned int j;
		for(j=i; j<i+cnt && j<max_cnt; ++j){
			if(bitmap_scan_test(btmp,j)==1)
				break;
		}
		if(j==i+cnt){                 //申请连续的cnt个空闲位并将其置1
			bitmap_set(btmp,i,cnt,1); 
			return i;
		}
	}
	return -1;
}

*/