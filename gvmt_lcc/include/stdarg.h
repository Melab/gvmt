#ifndef __STDARG
#define __STDARG

typedef union gvmt_reference_types ** va_list;

#define va_start(list, start) ((void)list=gvmt_stack_top())

#define va_arg(list, type) (sizeof(type)>sizeof(void*) ? \
    (*((type *)((list += 2)-2))) :  \
    (*((type *)((list += 1)-1))))

#define va_end(list) ((void)0)

typedef void *__gnuc_va_list;
#endif
