#ifndef __THREAD_ALLOC_H__
#define __THREAD_ALLOC_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void* thread_alloc(size_t size);
void* thread_realloc(void* addr, size_t size);
void  thread_free(void* addr);

#ifdef __cplusplus
}

#ifdef REDEFINE_DEFAULT_NEW_OPERATOR

void* operator new[]( size_t size );
void* operator new(size_t size);

void operator delete(void* addr);
void operator delete[]( void * p );

#endif
#endif


#endif
