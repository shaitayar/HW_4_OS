#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <sys/mman.h>

#define MAX_SIZE (1e8)
#define MMAP_TREASH (128*1024)
#define DEADBEEF (0xDEADBEEF)
#define MIN_SPLIT_SIZE (128)
int global_cookie = rand() % RAND_MAX;

size_t _size_meta_data();

class MallocMetadata{
    int cookie;
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;

public:
    MallocMetadata(size_t size, bool is_free):cookie(global_cookie), size(size), is_free(is_free), next(nullptr), prev(nullptr) {}
    MallocMetadata(size_t size, bool is_free, MallocMetadata* next, MallocMetadata * prev):
            cookie(global_cookie), size(size), is_free(is_free), next(next), prev(prev){}
    size_t getSize(){exit(DEADBEEF);this->checkCookie(); return this->size;}
    bool getIsFree(){this->checkCookie(); return is_free;}
    MallocMetadata* getNext(){this->checkCookie(); return next;}
    MallocMetadata* getPrev(){this->checkCookie(); return prev;}
    void* getMallocContent(){this->checkCookie(); return ((void*)(((char*)this) + _size_meta_data()));}

    void checkCookie(){if (cookie != global_cookie){exit(DEADBEEF);}}

    void setSize(size_t new_size){this->checkCookie(); this->size = new_size;}
    void setIsFree(bool new_is_free){this->checkCookie(); this->is_free = new_is_free;}
    void setNext(MallocMetadata* new_next){this->checkCookie(); this->next = new_next;}
    void setPrev(MallocMetadata* new_prev){this->checkCookie(); this->prev = new_prev;}
};

MallocMetadata* meta_data_list = nullptr;
MallocMetadata* meta_data_map_list = nullptr;
size_t map_list_length = 0;
size_t list_length = 0;

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
    MallocMetadata* lists[] = {meta_data_list, meta_data_map_list};
    for(int i=0; i<2; i++)
    {
        MallocMetadata* it = lists[i];
        while(it != nullptr)
        {
            num++;
            it = it->getNext();
        }
    }
    return num;
}


size_t _num_allocated_bytes(){
    size_t sum = 0;
    MallocMetadata* lists[] = {meta_data_list, meta_data_map_list};
    for(int i=0; i<2; i++)
    {
        MallocMetadata* it = lists[i];
        while(it != nullptr)
        {
            sum += it->getSize();
            it = it->getNext();
        }
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
void pushBackToMeta(MallocMetadata** list, MallocMetadata* new_meta)
{
    if (*list == nullptr)
        *list = new_meta;
    else
    {
        MallocMetadata* it = *list;
        while(it->getNext() != nullptr)
        {
            it = it->getNext();
        }
        it->setNext(new_meta);
        new_meta->setPrev(it);
    }
}

void sfree(void* p);

void deleteFromMeta(MallocMetadata** list ,MallocMetadata* old_meta)
{
    if (old_meta == nullptr)
        return;

    if (*list == old_meta)
    {
        *list = (*list)->getNext();
        if(*list != nullptr)
            (*list)->setPrev(nullptr);
    }
    else
    {
        MallocMetadata* it = (*list)->getNext();
        MallocMetadata* pre = *list;
        while(it != nullptr)
        {
            it->checkCookie();
            if (it == old_meta)
            {
                pre->setNext(it->getNext());
                if (it->getNext() != nullptr)
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
    size_t min_size = _num_free_bytes() + 1;
    MallocMetadata* it_min = nullptr;
    while(it != nullptr)
    {
        it->checkCookie();
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
        sfree(((void*)(((char*)new_meta_ptr) + _size_meta_data())));
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

/*
This function insert 0 to the next size bytes.
In case we iterate through several blocks it won't override the metadata, instead
it will fill with 0 all the data without the meta struct
*/
void insertZeroes(void* p, size_t size)
{
    memset(p, 0, size);
}

void deallocateMap(MallocMetadata* meta_ptr)
{
    deleteFromMeta(&meta_data_map_list, meta_ptr);
    munmap((void*)meta_ptr, meta_ptr->getSize());
}

void* allocateMap(size_t size)
{
    MallocMetadata new_meta = MallocMetadata(size, false);
    void* p = mmap(NULL, size + _size_meta_data(), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == nullptr)
        return nullptr;
    MallocMetadata* meta_ptr = ((MallocMetadata*)p);
    *meta_ptr = new_meta;
    pushBackToMeta(&meta_data_map_list, meta_ptr);
    return p;
}

void * smalloc (size_t size){
    if (size == 0 || size > MAX_SIZE) {return nullptr;}
    if (size >= MMAP_TREASH)
    {
        void* p = allocateMap(size);
        if (p == nullptr)
            return nullptr;
        return ((void*)(((char*)p) + _size_meta_data()));
    }

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
        wilderness->checkCookie();
        size_t increase_brk = size - wilderness->getSize();
        void* p = sbrk(increase_brk);
        if ((intptr_t)p == -1)
            return nullptr;
        wilderness->setIsFree(false);
        wilderness->setSize(increase_brk + wilderness->getSize());
        return (wilderness->getMallocContent()); // need to return the address excluding the meta struct
    }

    size_t size_with_meta = size + _size_meta_data();
    MallocMetadata new_meta = MallocMetadata(size, false);
    void* p = sbrk(size_with_meta);
    if((intptr_t)p == -1)
        return nullptr;
    MallocMetadata* meta_ptr = ((MallocMetadata*)p);
    *meta_ptr = new_meta;
    pushBackToMeta(&meta_data_list, meta_ptr);
    return meta_ptr->getMallocContent(); // need to return the address excluding the meta struct
}


//todo: the changes in s_malloc should effect scalloc?
void * scalloc (size_t num, size_t size)
{
    void* p = smalloc(num*size);
    if(p == nullptr)
        return nullptr;
    memset(p, 0, num*size);
    return p;
}

void sfree (void * p)
{
    if (p == nullptr)
        return;
    MallocMetadata* meta_ptr = ((MallocMetadata*)(((char*)p) - _size_meta_data()));
    meta_ptr->checkCookie();
    // If the block allocated by mmap
    if (meta_ptr->getSize() >= MMAP_TREASH)
    {
        deallocateMap(meta_ptr);
        return;
    }

    meta_ptr->setIsFree(true);
    MallocMetadata* meta_pre = meta_ptr->getPrev();
    MallocMetadata* meta_next = meta_ptr->getNext();
    size_t curr_size = meta_ptr->getSize() + _size_meta_data();

    if (meta_pre != nullptr)
    {
        if(meta_pre->getIsFree())
        {
            deleteFromMeta(&meta_data_list, meta_ptr);
            meta_pre->setSize(meta_pre->getSize() + curr_size);
            meta_ptr = meta_pre;
        }
    }
    if (meta_next != nullptr)
    {
        if(meta_next->getIsFree())
        {
            deleteFromMeta(&meta_data_list, meta_next);
            meta_ptr->setSize(meta_ptr->getSize() + meta_next->getSize() + _size_meta_data());
        }
    }
}

/*
 * This function delete the current block and inlarge the previues blocks,
 * In other words merge the current block with the previeus
 */
MallocMetadata* mergeWithLowerBlock(MallocMetadata* meta_ptr)
{

    MallocMetadata* pre = meta_ptr->getPrev();
    if (meta_ptr == nullptr || pre == nullptr)
        return nullptr;
    pre->checkCookie();
    deleteFromMeta(&meta_data_list, meta_ptr);
    pre->setSize(pre->getSize() + meta_ptr->getSize() + _size_meta_data());
    pre->setIsFree(false);
    return pre;
}

/*
 * This function delete the next block and inlarge the current block.
 * Merging the current with the next block
 */
MallocMetadata* mergeWithHigherBlock(MallocMetadata* meta_ptr)
{

    MallocMetadata* next = meta_ptr->getNext();
    if (meta_ptr == nullptr || next == nullptr)
        return nullptr;
    next->checkCookie();
    deleteFromMeta(&meta_data_list, next);
    meta_ptr->setSize(next->getSize() + meta_ptr->getSize() + _size_meta_data());
    return meta_ptr;
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
    old_meta->checkCookie();
    ///This case handle trying to realloc a map area
    if (old_meta->getSize() >= MMAP_TREASH)
    {
        if(old_meta->getSize() <= size)
            return oldp;
        void* new_p = smalloc(size);
        memmove(new_p, oldp, size);
        sfree(oldp);
        return new_p;
    }
    ///(a) This case reuse the current block without any merging
    if(old_meta->getSize() >= size) {
        splitBlock(old_meta, size);
        return oldp;
    }
    size_t old_size = old_meta->getSize();
    MallocMetadata* next = old_meta->getNext();
    size_t next_size = 0;
    MallocMetadata* pre = old_meta->getPrev();
    size_t pre_size = 0;
    MallocMetadata* new_meta;
    if (pre != nullptr)
    {
        if (pre->getIsFree())
        {
            pre_size = pre->getSize() + _size_meta_data();
            ///(b1)This case merging with lower address
            if(pre_size + old_size >= size)
            {
                new_meta = mergeWithLowerBlock(old_meta);
                splitBlock(new_meta, size);
                void* new_p = new_meta->getMallocContent();
                memmove(new_p, oldp, size);
                return new_p;
            }
            ///(b2)This case represents merging with lower and wilderness
            if(next == nullptr)
            {
                new_meta = mergeWithLowerBlock(old_meta);
                size_t increase_brk = size - new_meta->getSize();
                void* p = sbrk(increase_brk);
                if ((intptr_t)p == -1)
                    return nullptr;
                new_meta->setSize(increase_brk + new_meta->getSize());
                void* new_p = new_meta->getMallocContent();
                memmove(new_p, oldp, size);
                return new_p;
            }
        }
    }

    ///(c)This case is if the current block is the wilderness block
    if (next == nullptr)
    {
        size_t increase_brk = size - old_size;
        void* p = sbrk(increase_brk);
        if ((intptr_t)p == -1)
            return nullptr;
        old_meta->setSize(increase_brk + old_size);
        void* new_p = old_meta->getMallocContent();
        return new_p;
    }
    else
    {
        if (next->getIsFree())
        {
            next_size = next->getSize() + _size_meta_data();
            ///(d)This case represents merging with higher
            if(next_size + old_size >= size)
            {
                new_meta = mergeWithHigherBlock(old_meta);
                splitBlock(new_meta, size);
                void* new_p = new_meta->getMallocContent();
                memmove(new_p, oldp, size);
                return new_p;
            }
        }
    }
    ///(e)This case is merging with lower adress and higher address
    if(old_size + pre_size + next_size >= size)
    {
        new_meta = mergeWithHigherBlock(old_meta);
        new_meta = mergeWithLowerBlock(new_meta);
        splitBlock(new_meta, size);
        void* new_p = new_meta->getMallocContent();
        memmove(new_p, oldp, size);
        return new_p;
    }
    else
    {
        if (pre != nullptr && next != nullptr)
        {
            if(pre->getIsFree() && next->getIsFree())
                ///(f1)This case is merging lower and upper and the upper is wilderness
                if (next->getNext() == nullptr)
                {
                    new_meta = mergeWithHigherBlock(old_meta);
                    new_meta = mergeWithLowerBlock(new_meta);
                    size_t increase_brk = size - new_meta->getSize();
                    void* p = sbrk(increase_brk);
                    if ((intptr_t)p == -1)
                        return nullptr;
                    new_meta->setSize(increase_brk + new_meta->getSize());
                    new_meta->setIsFree(false);
                    void* new_p = new_meta->getMallocContent();
                    memmove(new_p, oldp, size);
                    return new_p;
                }
        }
        ///(f2)This case handle merging with higher and wilderness
        if (next->getNext() == nullptr)
        {
            new_meta = mergeWithHigherBlock(old_meta);
            size_t increase_brk = size - new_meta->getSize();
            void* p = sbrk(increase_brk);
            if ((intptr_t)p == -1)
                return nullptr;
            new_meta->setSize(increase_brk + new_meta->getSize());
            new_meta->setIsFree(false);
            void* new_p = new_meta->getMallocContent();
            return new_p;
        }
    }
    void* new_p = smalloc(size);
    if (new_p == nullptr)
        return nullptr;
    old_meta->setIsFree(true);
    memmove(new_p, oldp, size);
    return new_p;
}
//

void pirntData()
{
    std::cout << "aloc blocks:" << _num_allocated_blocks() << std::endl;
    std::cout << "aloc bytes:" << _num_allocated_bytes() << std::endl;
    std::cout << "free blocks" << _num_free_blocks() << std::endl;
    std::cout << "free bytes:" << _num_free_bytes() << std::endl << std::endl;

}
//int main() {
//
//    void* p = sbrk(0);
//    size_t head = _size_meta_data();
//    void* pad1 = (char *) smalloc(32);
//    void* a = (char *) smalloc(32);
//    void* b = (char *) smalloc(32);
//    void* c= (char*) smalloc(32);
//    pirntData();
//    sfree(a);
//    sfree(c);
//    pirntData();
//    void* new_b= (char*) srealloc(b, 32+4*_size_meta_data()*2);
//    if (a == new_b)
//        std::cout << "a = new_b" << std::endl;
//    pirntData();
//    sfree(new_b);
//    sfree(pad1);
//    std::cout << "new_b:" << (int*)new_b << std::endl;
//    std::cout << "a:" << (int*)a << std::endl;
//    std::cout << "b:" << (int*)b << std::endl;
//    std::cout << "c:" << (int*)c << std::endl;
////    void* new_2b= (char*) srealloc(new_b, 64 + _size_meta_data());
//
//    sfree(new_b);
//    pirntData();
//    MallocMetadata m=MallocMetadata(5,false);
//    m.getSize();
//    int d = 0;
//}
