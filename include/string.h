#pragma once

void memset(void* dest, unsigned char value, unsigned int size);
void memcpy(void* dest, const void* src, unsigned int size);
int memcmp(const void* a, const void* b,  unsigned int size);
char* strcpy(char* dest, const char* src);
unsigned int strlen(const char* str);
int strcmp(const char* a, const char* b);
char* strchr(const char* str, const char ch);
char* strrchr(const char* str, const char ch);
char* strcat(char* dest, const char* src);
unsigned int strchrs(const char* str, const char ch); //在字符串str中查找字符ch出现的次数


