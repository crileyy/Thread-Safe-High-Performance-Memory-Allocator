#ifndef HMEM_C
#define HMEM_C

#include <stdint.h>

typedef struct nu_free_cell {
    int64_t              size;
    struct nu_free_cell* next;
} nu_free_cell;

int64_t nu_free_list_length();
void nu_print_free_list();
static void nu_free_list_coalesce();
static void nu_free_list_insert(nu_free_cell* cell);
static nu_free_cell* free_list_get_cell(int64_t size);
static nu_free_cell* make_cell();
void* hmalloc(size_t usize);
void hfree(void* addr);
void* hrealloc(void* prev, size_t bytes);


#endif
