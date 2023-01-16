#include <stdio.h>
#include <unistd.h>

#define MAX_SIZE (1e8)

void * smalloc (size_t size){}

void * scalloc (size_t num, size_t size) {}

void sfree (void * p){}

void * srealloc(void * oldp, size_t size){}



size_t _num_free_blocks(){}

size_t _num_free_bytes(){}

size_t _num_allocated_blocks(){}

size_t _num_allocated_bytes(){}

size_t _num_meta_data_bytes(){}

size_t _size_meta_data(){}

