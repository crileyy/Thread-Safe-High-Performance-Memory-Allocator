// CS3650 CH02 starter code
// Fall 2017
//
// Author: Nat Tuck
// Once you've read this, you're done
// with HW07.


#include <stdint.h>
#include <sys/mman.h>
#include <assert.h>
#include <stdio.h>

#include <string.h>

#include <pthread.h>
#include "fast_malloc.h"
#include "math_helper.h"

static const int64_t CHUNK_SIZE = 200000000;  // 2 million
static const int64_t CELL_SIZE  = (int64_t)sizeof(nu_free_cell);

static nu_free_cell * global_bin = 0;

// this is arena
__thread nu_free_cell** bins = 0; // a pointer to nu_free_cells

static const int64_t INIT_BIN_SIZE = 3000000; // 3 mil
// __thread int init_bins = 0;

int counter = 0;
static const int64_t MAX_COUNTER = 100;
// works in binary from 0 -> 28
static const int64_t NUM_BINS = 28;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // Initialize the mutex


int64_t
nu_free_list_length()
{
    int len = 0;
    nu_free_cell* pp;
   
    int i;
    for (i = 0; i < NUM_BINS; i++)
    {
        for (pp = bins[i]; pp != 0; pp = pp->next) {
            len++;
         }
    }
    return len;
}

void
nu_print_free_list()
{
    printf("= Free list: =\n");
    
    int i;
    for(i = 0; i < NUM_BINS; i++)
    {   
        printf("Bin value: %d\n", (int)power(2, i));
        nu_free_cell* pp = bins[i];

        for (; pp != 0; pp = pp->next) {
            printf("%lx: (cell %ld %lx)\n", (int64_t) pp, pp->size, (int64_t) pp->next); 
        }
    }
}

static
void
nu_free_list_coalesce()
{
    int i;
    for (i = 0; i < NUM_BINS; i++)
    {
        nu_free_cell* pp = bins[i];
        int free_chunk = 0;
    
        while (pp != 0 && pp->next != 0) {
            if (((int64_t)pp) + pp->size == ((int64_t) pp->next)) {
                pp->size += pp->next->size;
                pp->next  = pp->next->next;
            }
    
            pp = pp->next;
        }
    }
}

static
void
nu_free_list_insert(nu_free_cell* cell)
{
    int64_t size = cell->size;
    long bit_num = ilog2((long) size) - 1;  // laws of logs 

    int64_t bit_size = power(2, bit_num);

    nu_free_cell* pp = bins[bit_num];

    if (pp == 0) {
        cell->next = bins[bit_num];
        bins[bit_num] = cell;

        return;
    }

    cell->next = pp->next;
    pp->next = cell;
}

static
nu_free_cell*
free_list_get_cell(int64_t size)
{

    long bit_num = ilog2((long)size) - 1;  // laws of logs 
    int i;
    for (i = bit_num; i < NUM_BINS; i++)
    {
        nu_free_cell ** prev = &(bins[i]);
        nu_free_cell * pp = bins[i];
        if (pp != 0)
        {
            *prev = pp->next;
            return pp;
        }
    }

    return 0;
}

static
nu_free_cell*
make_cell()
{
    void* addr = mmap(0, CHUNK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    nu_free_cell* cell = (nu_free_cell*) addr; 
    return cell;
}


void
initialize_global_bins()
{
    pthread_mutex_lock(&mutex);
    global_bin = mmap(0, CHUNK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    global_bin->size = CHUNK_SIZE;
    global_bin->next = 0;
    pthread_mutex_unlock(&mutex);
}

void
initialize_local_bin()
{
    // make the free list with bins of 0 -> 12 values, reping binary
    long sizeofbins = sizeof(nu_free_cell*) * NUM_BINS;
    pthread_mutex_lock(&mutex);

    int64_t size = global_bin->size;

    // give it space
    bins = (nu_free_cell **)global_bin;

    global_bin = (nu_free_cell*)((void*)global_bin + sizeofbins);

    // give it the some initial memory
    nu_free_cell * cell = global_bin;
    cell->size = INIT_BIN_SIZE;
    cell->next = 0;

    global_bin = (nu_free_cell*)((void*)global_bin + INIT_BIN_SIZE);
    global_bin->size = size - INIT_BIN_SIZE;
    global_bin->next = 0;
    pthread_mutex_unlock(&mutex);

    nu_free_list_insert(cell);
}

void*
opt_malloc(size_t usize)
{
    int64_t size = (int64_t) usize;

    // space for size
    int64_t alloc_size = size + sizeof(int64_t);

    // space for free cell when returned to list
    if (alloc_size < CELL_SIZE) {
        alloc_size = CELL_SIZE;
    }

    if (global_bin == 0)
    {
        initialize_global_bins();
    }
    if (bins == 0)
    {
        initialize_local_bin();
    }

    // Handle large allocations.
    if (alloc_size > CHUNK_SIZE) {
        void* addr = mmap(0, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        
        *((int64_t*)addr) = alloc_size;
        return addr + sizeof(int64_t);
    }

    nu_free_cell* cell = free_list_get_cell(alloc_size);
    int64_t rest_size;
    if (!cell) {
        cell = make_cell();
        cell->size = alloc_size;
        rest_size = CHUNK_SIZE - alloc_size;
    }
    else
    {
        rest_size = cell->size - alloc_size;
    }

    // Return unused portion to free list.
    if (rest_size >= CELL_SIZE) {
        void* addr = (void*) cell;
        nu_free_cell* rest = (nu_free_cell*) (addr + alloc_size);
        rest->size = rest_size;

        nu_free_list_insert(rest);
    }
    
    *((int64_t*)cell) = alloc_size;
    
    return ((void*)cell) + sizeof(int64_t);
}

void
opt_free(void* addr) 
{
    nu_free_cell* cell = (nu_free_cell*)(addr - sizeof(int64_t));
    int64_t size = *((int64_t*) cell);

    if (size > CHUNK_SIZE) { // bigger than we want to deal with
        munmap((void*) cell, size);
    }
    else { // put it on the free list
        cell->size = size;
        cell->next = 0;
        nu_free_list_insert(cell);
    }
}

void*
opt_realloc(void * prev, size_t bytes)
{
    /*
     * do reallocation of the previous to new bytes
     *
     * 1. unalloc the input
     * 2. mallocate space for it
     *   - this handles all the stuff
     * 3. then return
     */
    nu_free_cell* cell = (nu_free_cell*)(prev - sizeof(int64_t));
    int64_t size = *((int64_t*) cell);
    //allocate new block of memory
    void* block = opt_malloc(bytes);
    //copy data in prev to it
    memcpy(block, prev, size);
    opt_free(prev);

    return block;
}