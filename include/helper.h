#ifndef HELPER_H
#define HELPER_H

#define PROLOGUE 32 | THIS_BLOCK_ALLOCATED
#define BLOCK_SIZE_MASK 0xFFFFFFF0
#define PAYLOAD_SIZE_MASK 0xFFFFFFFF00000000
#define BITS 0xF
#define M 32

#include "sfmm.h"

double aggregate_payload;
double memory_utilization;
double peak_memory_utilization;

int init_heap();
void init_sf_free_list_heads();
void put_in_free_list(sf_block *free_block, int index);
void remove_from_free_list(sf_block *block, int index);
void init_quick_lists(); 
void create_footer(sf_block *block);
void *allocate_block(sf_block *free_block, sf_size_t size, sf_size_t payload, int index);
int is_relocatable(int index, sf_size_t remainder_block_size);
int find_new_index(sf_size_t remainder_block_size);
sf_size_t calculate_block_size(sf_size_t size);
int add_page();
sf_block *coalesce(sf_block *block, int prev_block_is_free, int next_block_is_free, int is_adding_page);
int belongs_in_sf_quick_lists(sf_size_t size);
void put_in_sf_quick_list(sf_block *remainder_block, int index);
void flush_sf_quick_list(sf_block *remainder_block, int index);
void calculate_memory_utilization();

void *get_next_block(void *block);
void *get_prev_block(void *block);
void to_binary(long unsigned int decimal);
void print_block_stats(sf_block *block);


#endif