/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "debug.h"
#include "sfmm.h"
#include "helper.h"

void *sf_malloc(sf_size_t size) {
    // TO BE IMPLEMENTED
    sf_block *allocated_block;
    sf_block *curr_block;
    sf_size_t curr_size;
    sf_size_t desired_block_size;
    int res;
    int found = 0;    // Has the appropriate free block been found yet?
    int i;
    int j;
    (void) j;


    if (size == 0)
    {
        debug("Returning NULL without setting sf_errno\n");
        sf_errno = ENOMEM;
        return NULL;
    }
    if (size > UINT32_MAX - 24)
    {
        debug("Returning NULL without setting sf_errno\n");
        sf_errno = ENOMEM;
        return NULL;
    }

    desired_block_size = calculate_block_size(size);
    
    // Initialize heap if fist time calling sf_malloc()
    void *start = sf_mem_start();
    void *end = sf_mem_end();

    if (start == end)
    {
        debug("Initializing heap...\n");
        if ((res = init_heap()) == -1)
            return NULL;
    }
    // sf_show_heap();

    // check for appropriate block in quick lists
    if ((res = belongs_in_sf_quick_lists(desired_block_size)) == 1)
    {
        debug("Looking for a block of size %d in sf_quick_lists\n", desired_block_size);
        int quick_lists_index = ((desired_block_size-M)/16);
        if (sf_quick_lists[quick_lists_index].first != NULL)
        {
            allocated_block = allocate_block(sf_quick_lists[quick_lists_index].first, desired_block_size, size, quick_lists_index);
            return allocated_block->body.payload;
        }
        debug("Failed to find a block of size %d in sf_quick_lists\n", desired_block_size);
    }

    // find appropriate sized block from free lists.
    while (!found)
    {
        for (i = 0; i < NUM_FREE_LISTS; i++)
        {
            curr_block = sf_free_list_heads[i].body.links.next; // points to first free block in each free list

            while (curr_block != &sf_free_list_heads[i])    // iterate through entire freelist
            {
                curr_size = (curr_block->header ^ MAGIC) & BLOCK_SIZE_MASK;

                if (desired_block_size <= curr_size)
                {
                    found = 1;
                    allocated_block = allocate_block(curr_block, desired_block_size, size, i);
                    // print_block_stats(allocated_block);
                    allocated_block = (sf_block *) ((void *) allocated_block - 16);     // points to prev_footer

                    // Check if allocated_block is the last block in the heap. If so, turn on 'prev alloc' bit in epilogue
                    if (((void *) allocated_block + ((allocated_block->header ^ MAGIC) & BLOCK_SIZE_MASK)) == (sf_mem_end() - 16))
                    {
                        debug("allocated_block is the last block in the heap. Update epilogue.\n");
                        sf_block *epilogue = (sf_block *) (sf_mem_end() - 16);
                        // epilogue->prev_footer = 0x0 ^ MAGIC;
                        epilogue->header ^= MAGIC;
                        epilogue->header |= PREV_BLOCK_ALLOCATED;
                        epilogue->header ^= MAGIC;
                        
                        // epilogue->header ^= MAGIC;
                    }

                    // Turn off the 'prv aloc' bit of the next block in heap.
                    sf_block *next_block = (sf_block *)((char *)allocated_block + ((allocated_block->header^MAGIC) & BLOCK_SIZE_MASK));
                    if ((void *) next_block < ((void *) sf_mem_end() - 16))
                    {
                        if (((next_block->header ^ MAGIC) & PREV_BLOCK_ALLOCATED) == 0)
                        {
                            next_block->header ^= MAGIC;
                            next_block->header |= PREV_BLOCK_ALLOCATED;
                            next_block->header ^= MAGIC;
                        }
                    }

                    // print_block_stats((sf_block*)((char *)next_block + 16));
                    calculate_memory_utilization();
                    return allocated_block->body.payload;
                }
                curr_block = curr_block->body.links.next;
            }
        }
        debug("Large enough block was not found, you must add a new page of memory.\n");
        if ((res=add_page())==-1)
        {
            debug("Setting sf_errno to ENOMEM and returning NULL.\n");
            sf_errno = ENOMEM;
            return NULL;
        }
    }

    return NULL;
    // abort();
}

void sf_free(void *pp) {
    // TO BE IMPLEMENTED
    sf_block *null_block = (sf_block *)pp;
    if (null_block == NULL)
    {
        debug("INVALID POINTER: pointer is null\n");
        abort();
    }

    pp -= 16;
    sf_block *block = (sf_block *) pp;
    sf_size_t block_size;
    long int block_address;
    int prev_block_allocated = 0;
    (void) prev_block_allocated;

    // debug("printing block stats:\n");
    // pp += 16;
    // block = (sf_block *) pp;
    // print_block_stats(block);
    
    // pp -= 16;
    // block = (sf_block *) pp;    // block is pointing to prev_footer

    // Abort if 'pp' points to the prologue or epilogue
    sf_header block_header = block->header ^ MAGIC;
    
    if (((block_header & PAYLOAD_SIZE_MASK) == 0) &&
        ((block_header & BLOCK_SIZE_MASK) == 32) &&
        ((block_header & THIS_BLOCK_ALLOCATED) != 0) &&
        (pp == sf_mem_start()))
    {
        debug("ERROR: The prologue cannot be freed\n");
        abort();
    }

    if (((block_header & THIS_BLOCK_ALLOCATED) != 0) &&
        ((block_header & PAYLOAD_SIZE_MASK) == 0) &&
        ((block_header & BLOCK_SIZE_MASK) == 0))
    {
        debug("ERROR: The epilogue cannot be freed\n");
        abort();
    }
    
    // if (block == NULL)
    // {
    //     debug("INVALID POINTER: pointer is null\n");
    //     abort();
    // }

    block_address = (long int) pp;
    if (block_address % 16 != 0)
    {
        debug("INVALID POINTER: pointer is not 16-byte aligned.");
        abort();
    }

    block_size = (block->header) ^ MAGIC;
    if ((block_size & THIS_BLOCK_ALLOCATED) == THIS_BLOCK_ALLOCATED)
    {
        block_size -= 4;
    }
    else
    {
        debug("INVALID POINTER: The allocated bit is 0.\n");
        abort();
    }

    if ((block_size & PREV_BLOCK_ALLOCATED) == PREV_BLOCK_ALLOCATED)
    {
        block_size -= 2;
        prev_block_allocated = 1;
    }

    if (block_size < 32 || (block_size % 16 != 0) ||
        (block_address < (unsigned long) sf_mem_start()) ||
        (unsigned long)((char *)block + block_size) > (unsigned long) sf_mem_end())
    {
        debug("INVALID POINTER: The block size is less than 32, or the block's address is not 16 bytes aligned.\n");
        debug("Or, the header of the block is before the start of the first block of the heap, or the footer of the block is after the end of the last block in the heap.\n");
        abort();
    }

    if (((block->header ^ MAGIC) & PREV_BLOCK_ALLOCATED) == 0)
    {
        
        sf_block *prev_block = get_prev_block((sf_block *) ((char *) block +16));
        unsigned long new_block_address = (unsigned long) prev_block;
        if ((new_block_address >= (unsigned long) sf_mem_start()) && (new_block_address < (unsigned long) sf_mem_end())
            && (((prev_block->header ^ MAGIC) & THIS_BLOCK_ALLOCATED) != 0))
        {
            debug("The prev_alloc field in the header is 0, indicating that the previous block is free, but the alloc field of the previous block header is not 0.");
            abort();
        }
    }
    
    // free the block
    // check if the block should belong in a quick list
    int res;
    sf_block *temp;
    (void) temp;
    if ((res=belongs_in_sf_quick_lists(block_size)) == 1)
    {
        debug("The block to be freed belongs in a quick list. Putting block of size %d in a quick list\n", block_size);
        int quick_list_index = (block_size-M)/16;
        block->header ^= MAGIC;
        block->header = block->header & 0x00000000FFFFFFFF;        // remove payload        
        block->header |= IN_QUICK_LIST;
        block->header ^= MAGIC;

        put_in_sf_quick_list(block, quick_list_index);
        // sf_show_heap();
        return;
    }

    // Or coalesce and put in a free list.
    debug("Block should be put in a free list. To be implemented later...\n");

    block = (sf_block *)((char *) block + 16);      // 'block' now points to its payload
    sf_block *prev_block = get_prev_block(block);   // returned 'prev_block' points to its prev_footer
    block = (sf_block *) pp;                        // 'block' now points to prev_footer
    
    // tmp -= 16;
    // prev_block = (sf_block *) tmp;

    int prev_block_is_free = prev_block->header ^ MAGIC;

    if ((prev_block_is_free & THIS_BLOCK_ALLOCATED) == 0)
        prev_block_is_free = 1;
    else
        prev_block_is_free = 0;
    
    prev_block = (sf_block *)((char *) prev_block + 16);    // prev_block points to payload
    // print_block_stats(prev_block);                          // before: payload; after: still payload

    block = (sf_block *)((char *) block + 16);              // block points to payload
    sf_block *next_block = get_next_block(block);           // returned next_block points to prev_footer

    int next_block_is_free = (next_block->header ^ MAGIC);
    if ((next_block_is_free & THIS_BLOCK_ALLOCATED) == 0)
        next_block_is_free = 1;
    else
        next_block_is_free = 0;

    next_block = (sf_block *) ((char *) next_block + 16);    // next_block points to payload
    // print_block_stats(next_block);                           // before: payload; after: still payload

    if (!prev_block_is_free && !next_block_is_free)
    {
        debug("No coalescing necessary\n");
        block = (sf_block *)((char *) block - 16);      // block points to prev_footer 
        block->header = (block->header ^ MAGIC) & ~(THIS_BLOCK_ALLOCATED);
        block->header &= 0x00000000FFFFFFFF;            // remove payload
        to_binary(block->header);
        block->header ^= MAGIC;
        create_footer(block);

        // put in a free list
        int free_list_index = find_new_index(block_size);
        put_in_free_list(block, free_list_index);

        // Turn off the 'prv alloc' bit in the next block's header
        sf_block *next_block = (sf_block *)((char *) block + ((block->header ^ MAGIC) & BLOCK_SIZE_MASK));
        next_block->header ^= MAGIC;
        next_block->header &= ~(PREV_BLOCK_ALLOCATED);
        next_block->header ^= MAGIC;
    }
    else
    {
        block = (sf_block *)((char *)block-16);     // block now points to prev_footer
        sf_block *new_block = coalesce(block, prev_block_is_free, next_block_is_free, 0);
        int new_block_size = (new_block->header ^ MAGIC) & BLOCK_SIZE_MASK;

        int free_list_index = find_new_index(new_block_size);
        (void) free_list_index;

        // put new block at the front of the free list
        sf_block *first_block = sf_free_list_heads[free_list_index].body.links.next;
        if (first_block == &sf_free_list_heads[free_list_index])
        {
            sf_free_list_heads[free_list_index].body.links.next = new_block;
            new_block->body.links.prev = &sf_free_list_heads[free_list_index];
            new_block->body.links.next = &sf_free_list_heads[free_list_index];    
        }
        else
        {
            // first_block->body.links.prev->body.links.next = new_block;
            sf_free_list_heads[free_list_index].body.links.next = new_block;
            // new_block->body.links.prev = first_block->body.links.prev;
            new_block->body.links.prev = &sf_free_list_heads[free_list_index];
            new_block->body.links.next = first_block;
        }
    }

    return;
}

void *sf_realloc(void *pp, sf_size_t rsize) {
    // TO BE IMPLEMENTED
    int prev_block_allocated = 0;
    (void) prev_block_allocated;

    sf_block *null_block = (sf_block *)pp;
    if (null_block == NULL)
    {
        debug("INVALID POINTER: pointer is null\n");
        sf_errno = EINVAL;
        return NULL;
    }

    // debug("printing block stats:\n");
    sf_block *block = (sf_block *) pp;
    // print_block_stats(block);

    pp -= 16;
    block = (sf_block *) pp;    // block is pointing to prev_footer

    // Abort if 'pp' points to the prologue or epilogue
    sf_header block_header = block->header ^ MAGIC;
    
    if (((block_header & PAYLOAD_SIZE_MASK) == 0) &&
        ((block_header & BLOCK_SIZE_MASK) == 32) &&
        ((block_header & THIS_BLOCK_ALLOCATED) != 0) &&
        (pp == sf_mem_start()))
    {
        debug("ERROR: The prologue cannot be freed\n");
        sf_errno = EINVAL;
        return NULL;
    }

    if (((block_header & THIS_BLOCK_ALLOCATED) != 0) &&
        ((block_header & PAYLOAD_SIZE_MASK) == 0) &&
        ((block_header & BLOCK_SIZE_MASK) == 0))
    {
        debug("ERROR: The epilogue cannot be freed\n");
        sf_errno = EINVAL;
        return NULL;
    }

    long int block_address = (long int) pp;
    if (block_address % 16 != 0)
    {
        debug("INVALID POINTER: pointer is not 16-byte aligned.");
        sf_errno = EINVAL;
        return NULL;
    }

    sf_size_t block_size = (block->header) ^ MAGIC;
    if ((block_size & THIS_BLOCK_ALLOCATED) == THIS_BLOCK_ALLOCATED)
    {
        block_size -= 4;
    }
    else
    {
        debug("INVALID POINTER: The allocated bit is 0.\n");
        sf_errno = EINVAL;
        return NULL;
    }

    if ((block_size & PREV_BLOCK_ALLOCATED) == PREV_BLOCK_ALLOCATED)
    {
        block_size -= 2;
        prev_block_allocated = 1;
    }

    if (block_size < 32 || (block_size % 16 != 0) ||
        (block_address < (unsigned long) sf_mem_start()) ||
        (unsigned long)((char *)block + block_size) > (unsigned long) sf_mem_end())
    {
        debug("INVALID POINTER: The block size is less than 32, or the block's address is not 16 bytes aligned.\n");
        debug("Or, the header of the block is before the start of the first block of the heap, or the footer of the block is after the end of the last block in the heap.\n");
        sf_errno = EINVAL;
        return NULL;
    }

    if (((block->header ^ MAGIC) & PREV_BLOCK_ALLOCATED) == 0)
    {
        
        sf_block *prev_block = get_prev_block((sf_block *) ((char *) block +16));
        unsigned long new_block_address = (unsigned long) prev_block;
        if ((new_block_address >= (unsigned long) sf_mem_start()) && (new_block_address < (unsigned long) sf_mem_end())
            && (((prev_block->header ^ MAGIC) & THIS_BLOCK_ALLOCATED) != 0))
        {
            debug("The prev_alloc field in the header is 0, indicating that the previous block is free, but the alloc field of the previous block header is not 0.");
            sf_errno = EINVAL;
            return NULL;
        }   
    }

    // Relocating to a larger size
    // pp -= 16;
    // sf_block *x = (sf_block *) pp;          // points to x's payload
    // sf_block *x = (sf_block *) (pp - 16);          // points to x's prev_footer
    sf_block *x = (sf_block *) pp;          // points to x's prev_footer
    if (rsize == 0)
    {
        debug("Pointer is valid, but reallocating to size 0. Will free the block and return NULL.\n");
        x = (sf_block *)((char *) x + 16);
        sf_free(x);
        return NULL;
    }
    sf_size_t request_block_size = calculate_block_size(rsize);


    if (request_block_size > ((x->header ^ MAGIC) & BLOCK_SIZE_MASK))
    {
        // sf_block *x = (sf_block *) pp;          // points to x's payload
        debug("Printing: \n");
        x = (sf_block *) ((char *) x + 16);    // points to x's payload
        // print_block_stats(x);                   // still points to x's payload
        
        sf_block *y = sf_malloc(rsize);         // y points to y's payload
        if (y == NULL)
        {
            debug("sf_realloc received NULL from sf_malloc, so sf_realloc is also returning NULL\n");
            return NULL;
        }
        memcpy(y, x, rsize);
        sf_free(x);
        calculate_memory_utilization();
        return y;
    }


    // Realocation to a smaller size where splitting causes a splinter
    else if ((((x->header ^ MAGIC) & BLOCK_SIZE_MASK) - request_block_size) < M)
    {
        debug("Realocation to a smaller size where splitting causes a splinter\n");
        // update the header
        x->header ^= MAGIC;
        x->header &= 0x00000000FFFFFFFF;
        x->header |= (((uint64_t)rsize)<<32);
        x->header ^= MAGIC;
        calculate_memory_utilization();
        return x->body.payload;
    }

    // Realocation to a smaller size where splitting does not cause a splinter
    else if ((((x->header ^ MAGIC) & BLOCK_SIZE_MASK) - request_block_size) >= M)
    {
        debug("Realocation to a smaller size where splitting DOES NOT cause a splinter\n");
        sf_size_t block_size = (x->header ^ MAGIC) & BLOCK_SIZE_MASK;
        sf_block *allocated_block = x;
        sf_block *remainder_block;

        // Create the allocated block's new header
        uint32_t allocated_block_bits = (x->header ^ MAGIC) & BITS;
        uint64_t allocated_block_header = (((uint64_t)rsize)<<32);      // payload
        allocated_block_header |= request_block_size;                   // block size
        allocated_block_header |= allocated_block_bits;                 // bits
        allocated_block_header ^= MAGIC;
        allocated_block->header = (sf_header) allocated_block_header;
        allocated_block->prev_footer = x->prev_footer;

        // Create the remainder block's new header
        remainder_block = (sf_block *)((char *) allocated_block + request_block_size);    // points to prev_footer
        remainder_block->header = 0x0;
        remainder_block->header = ((block_size - request_block_size) | PREV_BLOCK_ALLOCATED) ^ MAGIC;
        remainder_block->prev_footer = allocated_block->header;

        create_footer(remainder_block);         // before: prev_footer; after: still prev_footer

        // Coalesce the remainder_block if possible
        // Check if remainder block's next block is free
        remainder_block = (sf_block *)((char *) remainder_block + 16);  // remainder_block points to its payload
        sf_block *next_block = get_next_block(remainder_block);         // next_block points to prev_footer
        if (((next_block->header ^ MAGIC) & THIS_BLOCK_ALLOCATED) == 0)
        {
            remainder_block = (sf_block *)((char *) remainder_block - 16);  // remainder_block points to its prev_footer
            remainder_block = coalesce(remainder_block, 0, 1, 0);
            remainder_block = (sf_block *)((char *) remainder_block + 16);  // remainder_block points to its paylod
        }
        else
        {
            // turn off the 'prev alloc' bit of next block in heap
            // print_block_stats((sf_block *)((char *)next_block + 16));
            next_block->header ^= MAGIC;
            next_block->header &= ~(PREV_BLOCK_ALLOCATED);
            next_block->header ^= MAGIC;
            // print_block_stats((sf_block *)((char *)next_block + 16));
        }


        // add remainder_block to free list
        remainder_block = (sf_block *)((char *) remainder_block - 16);  // remainder_block points to its prev_footer
        sf_size_t remainder_block_size = (remainder_block->header ^ MAGIC) & BLOCK_SIZE_MASK;
        int free_list_index = find_new_index(remainder_block_size);
        put_in_free_list(remainder_block, free_list_index);

        calculate_memory_utilization();
        return allocated_block->body.payload;

    }
    // abort();
    return NULL;
}

double sf_internal_fragmentation() {
    // TO BE IMPLEMENTED
    void *p = sf_mem_start();
    double aggregate_payload = 0;
    double aggregate_block_size = 0;
    double internal_fragmentation = 0.0;

    // If heap is uninitialized, return 0.0
    if ((sf_mem_start() == sf_mem_end()))
    {
        debug("Heap is uninitialized.\n");
        return 0.0;
    }
    
    sf_block *curr_block = (sf_block *) p;

    // while ((((curr_block->header ^ MAGIC) & BLOCK_SIZE_MASK) != 0) &&
    //         (((curr_block->header ^ MAGIC) & PAYLOAD_SIZE_MASK) != 0))
    // {
    //     ;   
    // }

    while (p != (sf_mem_end() - 16))
    {
        // prologue does not count toward calculations
        // if ((((curr_block->header ^ MAGIC) & PAYLOAD_SIZE_MASK) == 0) &&
        // (((curr_block->header ^ MAGIC) & BLOCK_SIZE_MASK) == 32) &&
        // (((curr_block->header ^ MAGIC) & THIS_BLOCK_ALLOCATED) != 0))
        // {
        //     debug("The prologue does not count toward calculations\n");
        // }
        if (p == sf_mem_start())
        {
            debug("The prologue does not count toward calculations\n");
        }
        else if (((curr_block->header ^ MAGIC) & IN_QUICK_LIST) != 0)
        {
            debug("Blocks in quick lists do not count toward calculations\n");
        }
        else if (((curr_block->header ^ MAGIC) & THIS_BLOCK_ALLOCATED) == 0)
        {
            debug("Block at <%p> is free and will not count towards internal fragmentation calculations.\n", curr_block);
        }
        else
        {
            debug("Block at <%p> is allocated.\n", curr_block);
            // print_block_stats((sf_block *)((char *) curr_block + 16));
            aggregate_payload += (((curr_block->header ^ MAGIC) & PAYLOAD_SIZE_MASK) >> 32);
            aggregate_block_size += ((curr_block->header ^ MAGIC) & BLOCK_SIZE_MASK);
        }
        debug("aggregate_payload so far: %f\n", aggregate_payload);
        debug("aggregate_block_size so far: %f\n", aggregate_block_size);
        p += ((curr_block->header ^ MAGIC) & BLOCK_SIZE_MASK);
        curr_block = (sf_block *) p;
    }

    if (aggregate_payload == 0.0)
    {
        internal_fragmentation = 0.0;
    }
    else
    {
        internal_fragmentation = aggregate_payload/aggregate_block_size;
    }
    debug("The internal fragmentation is %f\n", internal_fragmentation);
    return internal_fragmentation;
    // abort();
}

double sf_peak_utilization() {
    // TO BE IMPLEMENTED
    // debug("The peak utilization is: %d\n", peak_memory_utilization);
    return peak_memory_utilization;
    // abort();
}
