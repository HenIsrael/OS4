#include <unistd.h>
#include <iostream>

#define MAX_ALLOCATE 100000000

void* smalloc (size_t size)
{
    if(size==0 || size>MAX_ALLOCATE)
    {
        return NULL;
    }
    void* beginBlock = sbrk(size);
    if (beginBlock == (void*)-1)
    {
        return NULL;
    }
    return beginBlock;
}