#ifndef STDDEF_H
#define STDDEF_H

typedef unsigned long  size_t;

typedef long ptrdiff_t;

typedef unsigned short wchar_t;

#undef NULL
#define NULL 0

#undef offsetof
#define offsetof(t,f)  (((char*)&(((t*)((void*)0))->f))-((char*)((void*)0)))

#endif 
