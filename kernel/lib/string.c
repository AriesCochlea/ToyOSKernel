#include "string.h"
#include "stdint.h"


void memset(void* dest, unsigned char value, unsigned int size){
	if(dest ==NULL)
		return;
    for(unsigned char* dst = (unsigned char*) dest; size>0; --size){
    	*dst = value;
    	++ dst;
    }
    return;
}


void memcpy(void* dest, const void* src, unsigned int size){
	if(dest==NULL || src==NULL)
		return;
    unsigned char* dst = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while(size > 0){
    	*dst = *s;
    	++dst;
    	++s;
    	--size;
    }
    return;
}


int memcmp(const void* a, const void* b,  unsigned int size){
	if(a==NULL && b==NULL)
		return 0;
	else if(a==NULL && b!=NULL)
		return -1;
	else if(a!=NULL && b==NULL)
		return 1;
		
    const unsigned char* _a = (const unsigned char*)a;
    const unsigned char* _b = (const unsigned char*)b;
    while(size > 0){
    	if(*_a != *_b)	
    		return (*_a > *_b) ? 1 : -1;
   		++_a;
   		++_b;
   		--size;
    }
    return 0;
}


char* strcpy(char* dest, const char* src){
	if(dest==NULL || src==NULL)
		return dest;
    char* d = dest;
    while(*src!='\0'){
    	*d = *src;
    	++d;
    	++src;
    }
    *d='\0';
    return dest;     
}


unsigned int strlen(const char* str){
	if(str==NULL)
		return 0;
    unsigned int len=0;
    while(*str!='\0'){
    	++len;
    	++str;
    }
    return  len;  
}


int strcmp(const char* a, const char* b){
	if(a==NULL && b==NULL)
		return 0;
	else if(a==NULL && b!=NULL)
		return -1;
	else if(a!=NULL && b==NULL)
		return 1;
    while(*a!='\0'&& *b!='\0' && *a == *b){
    	++a;
    	++b;
    }   
    if(*a == *b)
    	return 0;
    else
    	return (*a < *b) ? -1 : 1; 
}


char* strchr(const char* str, const char ch){
	if(str==NULL)
		return NULL;
    while(*str!='\0'){
    	if(*str == ch)	
    		return (char*)str;
    	++str;
    } 
    return NULL;
}


char* strrchr(const char* str, const char ch){
	if(str==NULL)
		return NULL;
    char* last = NULL;
    while(*str != '\0'){
    	if(ch == *str)	
    		last = (char*)str;
    	str++;
    }
    return last;   
}


char* strcat(char* dest, const char* src){
	if(dest==NULL || src==NULL)
		return dest;
    char* str = dest;
    while(*str!='\0')
    	++str;
    while(*src!='\0'){
    	*str = *src;
    	++str;
    	++src;
    }	
    *str = '\0';
    return dest;
}


unsigned int strchrs(const char* str, const char ch){
	if(str==NULL)
		return 0;
	unsigned int count = 0;
	while(*str != '\0'){
		if(ch == *str)
			++count;
		++str;
	}
    return count;
}

