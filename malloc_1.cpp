#include <stdio.h>
#include <unistd.h>

#define MAX_SIZE (1e8)
#define PAGE_SIZE (4096)

void* smalloc(size_t size){
    if(size<=0 || size > MAX_SIZE) return nullptr;
    //intptr_t increment =  (size + (PAGE_SIZE - size % PAGE_SIZE)) ;

    //append heap
    intptr_t p = (intptr_t)sbrk(size);

    if(p == -1){
        return nullptr;
    }

    return (void*)(p);
}

