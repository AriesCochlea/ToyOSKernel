#include "list.h"
#include "interrupt.h"
#include "stdint.h"



//初始化双向链表
void list_init(struct list* list)
{   enum intr_status old_status=intr_disable();
    list->head.prev = NULL;
    list->head.next = &list->tail;
    list->tail.prev = &list->head;
    list->tail.next = NULL;
    intr_set_status(old_status);
}

//把链表元素elem放在 before前面
void list_insert_before(struct list_elem* before,struct list_elem* elem)
{
    enum intr_status old_status=intr_disable();
    before->prev->next=elem;
    elem->prev=before->prev;
    elem->next=before;
    before->prev=elem;
    intr_set_status(old_status);
}

//添加元素到链表队首
void list_push(struct list* plist,struct list_elem* elem)
{
    list_insert_before(plist->head.next,elem);
}

//添加元素到链表队尾
void list_append(struct list* plist,struct list_elem* elem)
{
    list_insert_before(&plist->tail,elem);
}

//让pelem脱离链表
void list_remove(struct list_elem* pelem)
{
    enum intr_status old_status = intr_disable();
    
    pelem->prev->next = pelem->next;
    pelem->next->prev = pelem->prev;
    
    intr_set_status(old_status);
}

//让链表的第一个元素脱离链表
struct list_elem* list_pop(struct list* plist)
{
    struct list_elem* elem = plist->head.next;
    list_remove(elem);
    return elem;
}

int list_empty(struct list* plist)
{
    return (plist->head.next == &plist->tail ? 1 : 0);
}

uint32_t list_len(struct list* plist)
{
    struct list_elem* elem=plist->head.next;
    uint32_t length=0;
    while (elem!=&plist->tail){
        ++length;
        elem=elem->next;
    }
    return length;
}




/*
把链表plist中的每个元素elem和arg传给回调函数func,
参数arg给func函数用来判断elem是否符合条件
本函数的功能是遍历链表内所有元素，逐个判断是否有符合条件的元素。找到符合条件的元素返回元素指针，否则返回NULL
*/
struct list_elem* list_traversal(struct list* plist,function func,int arg)
{
    struct list_elem* elem = plist->head.next;
    if(list_empty(plist))	return NULL;
    while(elem != &plist->tail)
    {
    	if(func(elem,arg))
        	return elem;
    	elem = elem->next;
    }
    return NULL;
}


/*
在链表plist中查找obj_elem元素
如果找到obj_elem元素，返回1
如果未找到obj_elem元素，返回0
*/
int elem_find(struct list* plist,struct list_elem* obj_elem)
{
    struct list_elem* ptr = plist->head.next;
    while(ptr != &plist->tail)
    {
    	if(ptr == obj_elem)
        	return 1; 
    	ptr = ptr->next;
    }
    return 0;
}
