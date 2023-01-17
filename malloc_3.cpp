#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#define MAX_SIZE (1e8)



class MallocMetadata{
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;

public:
    MallocMetadata(size_t size, bool is_free):size(size), is_free(is_free), next(nullptr), prev(nullptr){}
    MallocMetadata(size_t size, bool is_free, MallocMetadata* next, MallocMetadata * prev):
    size(size), is_free(is_free), next(next), prev(prev){}
    size_t getSize(){return this->size;}
    bool getIsFree(){return is_free;}
    MallocMetadata* getNext(){return next;}
    MallocMetadata* getPrev(){return prev;}

    void setSize(size_t new_size){this->size = new_size;}
    void setIsFree(bool new_is_free){this->is_free = new_is_free;}
    void setNext(MallocMetadata* new_next){this->next = new_next;}
    void setPrev(MallocMetadata* new_prev){this->prev = new_prev;}

};

MallocMetadata* meta_data_list = nullptr;

size_t _num_free_blocks(){
    size_t num_free = 0;
    MallocMetadata* it = meta_data_list;
    while(it != nullptr){
        if (it->getIsFree())
            num_free++;
        it = it->getNext();
    }
    return num_free;
}

size_t _num_free_bytes(){
    size_t sum_free = 0;
    MallocMetadata* it = meta_data_list;
    while(it != nullptr){
        if (it->getIsFree())
            sum_free += it->getSize();
        it = it->getNext();
    }
    return sum_free;
}

size_t _num_allocated_blocks(){
    size_t num = 0;
    MallocMetadata* it = meta_data_list;
    while(it != nullptr){
        num++;
        it = it->getNext();
    }
    return num;
}

size_t _num_allocated_bytes(){
    size_t sum = 0;
    MallocMetadata* it = meta_data_list;
    while(it != nullptr){
        sum += it->getSize();
        it = it->getNext();
    }
    return sum;
}

size_t _num_meta_data_bytes(){
    size_t num_allocated = _num_allocated_blocks();
    return num_allocated*sizeof(*meta_data_list);
}

size_t _size_meta_data(){
    return sizeof(*meta_data_list);
}



/*
This function add a metadata struct to the end of the metadatalist
*/
void pushBackToMeta(MallocMetadata* new_meta)
{
    if (meta_data_list == nullptr)
        meta_data_list = new_meta;
    else{
        MallocMetadata* it = meta_data_list;
        while(it->getNext() != nullptr)
        {
            it = it->getNext();
        }
        it->setNext(new_meta);
        new_meta->setPrev(it);
    }
}

void deleteFromMeta(MallocMetadata* old_meta)
{
    if (old_meta == nullptr)
        return;

    if (meta_data_list == old_meta)
    {
        meta_data_list = meta_data_list->getNext();
        if(meta_data_list != nullptr)
            meta_data_list->setPrev(nullptr);
    }
    else
    {
        MallocMetadata* it = meta_data_list->getNext();
        MallocMetadata* pre = meta_data_list;
        while(it != nullptr)
        {
            if (it == old_meta)
            {
                pre->setNext(it->getNext());
                (it->getNext())->setPrev(pre);
                return;
            }
            pre = it;
            it = it->getNext();
        }
    }
}

/*
This function iterate over the meta data list and search for a block
that is free and also big enough to contain size in it.
In case of success it returns the adress of the block found,
In casse of failure it returns a nullptr
*/

size_t min(size_t x, size_t y)
{
    return x<=y ? x : y;
}

/*
 * This function find the fittest block to size of bytes
 * Returns a pointer to the fittest block found.
 */
void* findFreeBlock(size_t size)
{
    MallocMetadata* it = meta_data_list;
    size_t min_size = _num_free_bytes();
    MallocMetadata* it_min = nullptr;
    while(it != nullptr)
    {
        if (it->getIsFree() && it->getSize()>=size)
        {
            if (it->getSize() < min_size)
            {
                min_size = it->getSize();
                it_min = it;
            }
        }
        it = it->getNext();
    }
    return it_min;
}


/*
 * This function gets a pointer to a block and split the block if it's possible
 */
void splitBlock(MallocMetadata* ptr, size_t size)
{
    if(ptr->getSize() >= (128 + _size_meta_data() + size))
    {
        size_t next_block_size = ptr->getSize() - (_size_meta_data() + size);
        MallocMetadata new_meta = MallocMetadata(next_block_size, true, ptr->getNext(), ptr);

        void* enter_new_meta = ((char*)ptr) + size + _size_meta_data();
        MallocMetadata* new_meta_ptr = (MallocMetadata*)enter_new_meta;
        *new_meta_ptr = new_meta;

        ptr->setSize(size);
        if (ptr->getNext() != nullptr)
            (ptr->getNext())->setPrev(new_meta_ptr);
        ptr->setNext(new_meta_ptr);

    }
}

/*
 * This function returns the topmost Meta struct in the heap
 */
MallocMetadata* getWilderness()
{
    MallocMetadata* it = meta_data_list;
    if (it == nullptr)
        return nullptr;
    while(it->getNext() != nullptr)
        it = it->getNext();
    return it;
}
// /*
// This function finds size of freed bytes in a row and return a pointer to the metadata of the first block
// out of the sequence of blocks it found
// */
// void* findFreeBlocks(size_t num, size_t size)
// {
//     MallocMetadata* it = meta_data_list;
//     MallocMetadata* begin = meta_data_list;
//     size_t block_sum = 0;
//     while(it != nullptr)
//     {
//         if(it->getIsFree())
//         {
//             if(block_sum == 0)
//             {
//                 block_sum += it->getSize();
//                 begin = it;
//             }
//             else
//                 block_sum += it->getSize();
//             if(block_sum >= num)
//                 return begin;
//         }
//         else
//             block_sum = 0;
//         it = it->getNext();
//     }
//     return nullptr;
// }

/*
This function insert 0 to the next size bytes.
In case we iterate through several blocks it won't override the metadata, instead
it will fill with 0 all the data without the meta struct
*/
void insertZeroes(void* p, size_t size)
{
    memset(p, 0, size);
}

void * smalloc (size_t size){
    if (size == 0 || size > MAX_SIZE) {return nullptr;}

    if (_num_free_bytes() >= size)
    {
        void* p = findFreeBlock(size);
        if (p != nullptr)
        {
            ((MallocMetadata*)p)->setIsFree(false);
            splitBlock((MallocMetadata*)p, size);
            return ((void*)(((char*)p) + _size_meta_data())); // need to return the address excluding the meta struct
        }
    } // could not find free block in the proper size, therfore use sbrk

    // Wilderness check
    MallocMetadata* wilderness = getWilderness();
    if (wilderness != nullptr && wilderness->getIsFree())
    {
        size_t increase_brk = size - wilderness->getSize();
        void* p = sbrk(increase_brk);
        if ((intptr_t)p == -1)
            return nullptr;
        wilderness->setSize(increase_brk + wilderness->getSize());
        return ((void*)(((char*)wilderness) + _size_meta_data())); // need to return the address excluding the meta struct
    }

    size_t size_with_meta = size + _size_meta_data();
    MallocMetadata new_meta = MallocMetadata(size, false);
    void* p = sbrk(size_with_meta);
    if((intptr_t)p == -1)
        return nullptr;
    MallocMetadata* meta_ptr = ((MallocMetadata*)p);
    *meta_ptr = new_meta;
    pushBackToMeta(meta_ptr);
    return ((void*)(((char*)p) + _size_meta_data())); // need to return the address excluding the meta struct
}


//todo: the changes in s_malloc should effect scalloc?
void * scalloc (size_t num, size_t size)
{
    void* p = smalloc(num*size);
    if(p == nullptr)
        return nullptr;
    memset(p, 0, num*size);
    return p;
//    if (_num_free_bytes() >= num*size)
//    {
//        void* p = findFreeBlock(num*size);
//        if (p != nullptr)
//        {
//            void* inside_data = (void*)(((char*)p) + _size_meta_data());
//            insertZeroes(inside_data, num*size);
//            ((MallocMetadata*)p)->setIsFree(false);
//            return ((char*)p) + _size_meta_data();
//        }
//    }
//
//    size_t size_with_meta = size*num + _size_meta_data();
//    MallocMetadata new_meta = MallocMetadata(size, false);
//    void* p = sbrk(size_with_meta);
//    if((intptr_t)p == -1)
//        return nullptr;
//    MallocMetadata* meta_ptr = (MallocMetadata*)p;
//    *meta_ptr = new_meta;
//    pushBackToMeta(meta_ptr);
//    return ((char*)p) + _size_meta_data();

}

void sfree (void * p)
{
    if (p == nullptr)
        return;
    MallocMetadata* meta_ptr = ((MallocMetadata*)(((char*)p) - _size_meta_data()));
    meta_ptr->setIsFree(true);
    MallocMetadata* meta_pre = meta_ptr->getPrev();
    MallocMetadata* meta_next = meta_ptr->getNext();
    size_t curr_size = meta_ptr->getSize() + _size_meta_data();
    if (meta_pre != nullptr)
    {
        if(meta_pre->getIsFree())
        {
            deleteFromMeta(meta_ptr);
            meta_pre->setSize(meta_pre->getSize() + curr_size);
            meta_ptr = meta_pre;
        }
    }
    if (meta_next != nullptr)
    {
        if(meta_next->getIsFree())
        {
            deleteFromMeta(meta_next);
            meta_ptr->setSize(meta_ptr->getSize() + meta_next->getSize() + _size_meta_data());
        }
    }
}

//todo: what do we need to change in realloc>?
void * srealloc(void * oldp, size_t size)
{
    if (size == 0 || size > MAX_SIZE) {return nullptr;}

    if(oldp == nullptr)
    {
        void* new_p = smalloc(size);
        return new_p;
    }
    MallocMetadata* old_meta = (MallocMetadata*) ((((char*)oldp) - _size_meta_data()));
    if(old_meta->getSize() >= size) {
        return oldp;
    }
    void* new_p = smalloc(size);
    if (new_p == nullptr)
        return nullptr;
    old_meta->setIsFree(true);
    memmove(new_p, oldp, old_meta->getSize());
    return new_p;
}


//int main() {
//
//    void* p = sbrk(0);
//    void* a = smalloc(10);
//    std::cout << "a: " << (int*)a << std::endl;
//}