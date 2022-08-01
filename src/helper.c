#include <stdio.h>
#include <limits.h>

#include "debug.h"
#include "helper.h"

double aggregate_payload = 0.0;
double memory_utilization = 0.0;
double peak_memory_utilization = 0.0;

int init_heap()
{
    init_sf_free_list_heads();
    void *grow;
    
    if ((grow = sf_mem_grow()) == NULL)
    {
        debug("Not enough memory!\n");
        return -1;
    }
    sf_block *prologue;
    sf_block *epilogue;
    sf_block *first_block;

    // create prologue
    prologue = (sf_block *) grow;
    prologue->prev_footer = 0x0 ^ MAGIC;
    prologue->header = ((sf_header) PROLOGUE) ^ MAGIC ;

    // create the first block
    grow += M;
    first_block = (sf_block *) grow;
    first_block->prev_footer = prologue->header;
    first_block->header = ((sf_header) (PAGE_SZ-48) | PREV_BLOCK_ALLOCATED) ^ MAGIC; // prologue block size: 32; epilogue (just a single mem row + prev_footer) size: 16; 32 + 16 = 48

    create_footer(first_block);
    
    // place first block into free list
    sf_free_list_heads[5].body.links.next = first_block;
    sf_free_list_heads[5].body.links.prev = first_block;
    first_block->body.links.next = &sf_free_list_heads[5];
    first_block->body.links.prev = &sf_free_list_heads[5];

    // create the epilogue
    grow += (first_block->header ^ MAGIC) & BLOCK_SIZE_MASK;
    epilogue = (sf_block *) grow;
    epilogue->prev_footer = *((sf_footer *) grow);  // epilogue's prev_footer is assigned to where the pointer 'grow' points to in memory (i.e. first_block's footer's value).
    epilogue->header = (0x0 | THIS_BLOCK_ALLOCATED) ^ MAGIC;

    return 0;
}

void init_sf_free_list_heads()
{
    for (int i=0; i<NUM_FREE_LISTS; i++) {
        sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
        sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
        // sf_show_free_list(i);
    }
}

/** 'free_block' will point to its prev_footer. 'index' is the index of sf_free_list_heads
 *  which the free block will be put into.
 * 
 */
void put_in_free_list(sf_block *free_block, int index)
{
    // put free block at the front of the free list
    sf_block *first_block = sf_free_list_heads[index].body.links.next;
    if (first_block == &sf_free_list_heads[index])
    {
        sf_free_list_heads[index].body.links.next = free_block;
        free_block->body.links.prev = &sf_free_list_heads[index];
        free_block->body.links.next = &sf_free_list_heads[index];    
    }
    else
    {
        // first_block->body.links.prev->body.links.next = block;
        // block->body.links.prev = first_block->body.links.prev;
        // block->body.links.next = first_block->body.links.next;

        sf_block *old_first_block = sf_free_list_heads[index].body.links.next;
        sf_free_list_heads[index].body.links.next = free_block;
        // sf_free_list_heads[index].body.links.prev = old_first_block;
        free_block->body.links.prev = &sf_free_list_heads[index];
        free_block->body.links.next = old_first_block;    
        old_first_block->body.links.prev = free_block;
        // old_first_block->body.links.next = &sf_free_list_heads[index];
    } 
    return;
}

void remove_from_free_list(sf_block *block, int index)
{
    sf_block *curr_block = sf_free_list_heads[index].body.links.next;
    // sf_show_free_list(index);
    // traverse free list until block_header is found
    while (curr_block != block)
    {
        debug("curr_block: %p\n", curr_block);
        curr_block = curr_block->body.links.next;
    }
    // remove from free list
    sf_block* prev = curr_block->body.links.prev;
    prev->body.links.next = block->body.links.next;
    block->body.links.next = NULL;
    block->body.links.prev = NULL; 
    return;
}

void init_quick_lists()
{
    int i;

    for (i = 0; i < NUM_QUICK_LISTS; i++)
    {
        sf_quick_lists[i].length = 0;
        sf_quick_lists[i].first = NULL;
    }
}

/** expects a pointer to the block's prev_footer
 * 
 */
void create_footer(sf_block *block)
{
    size_t offset = (block->header ^ MAGIC) & BLOCK_SIZE_MASK;  // de-obfuscate before reading
    debug("offset: %ld\n", offset);
    void *footer = block;

    // iterate footer by offset
    footer += sizeof(char) * offset;

    *((sf_footer *) footer) = (sf_footer) block->header;
    debug("*((sf_footer *) footer): %lu\n", *((sf_footer *) footer) ^ MAGIC);
    to_binary(*((sf_footer *) footer) ^ MAGIC);

    // decrement 'footer' pointer
    footer -= sizeof(char) * offset;
    return;
}

/** Description: Allocates 'free_block' by changing its header. If splitting the block causes a
 *  splinter, the block is not split. Else, the block is split. The remaining block's
 *  header is changed and relocated to another free list if necessary. 
 * 
 *  Arguments:  
 *      'free_block' is the sf_block pointer to an appropriately sized free block in the heap
 *  or quick list.
 *      'size' is the block_size of 'free_block'.
 *      'index' is the index of sf_free_list_heads array in which 'free_block' resides.
 *
 *  Returns a sf_block pointer to the allocated block. The pointer points
 *  to the block's payload. This function will never return NULL.
 */
void *allocate_block(sf_block *free_block, sf_size_t size, sf_size_t payload, int index)
{
    sf_size_t block_size = (free_block->header ^ MAGIC) & BLOCK_SIZE_MASK;
    debug("block_size: %d\n", block_size);
    to_binary(block_size);
    debug("free_block->header: %ld\n", free_block->header ^ MAGIC);
    to_binary(free_block->header ^ MAGIC);

    int in_quick_list = (free_block->header ^ MAGIC) & IN_QUICK_LIST;
    uint64_t tmp;

    if (in_quick_list)
    {
        debug("Free block is in quick_list. To be implemented...\n");
        
        // remove first block from quick list
        sf_block *first_block = sf_quick_lists[index].first;
        sf_quick_lists[index].first = first_block->body.links.next;
        sf_quick_lists[index].length--;
        first_block->body.links.next = NULL;

        // turn off 'in qklst' and 'alloc' bit
        first_block->header = ((first_block->header ^ MAGIC) & ~(1) & ~(1<<2)) ^ MAGIC;
        debug("first_block->header: %lu\n", first_block->header ^ MAGIC);
        to_binary(first_block->header ^ MAGIC);
        // allocate first_block
        tmp = payload;
        debug("tmp: %ld\n", tmp);
        to_binary(tmp);
        tmp = tmp << 32;
        debug("tmp: %ld\n", tmp);
        to_binary(tmp);
        first_block->header = ((first_block->header ^ MAGIC) | THIS_BLOCK_ALLOCATED | tmp) ^ MAGIC;
        debug("first_block->header: %lu\n", first_block->header ^ MAGIC);
        to_binary(first_block->header ^ MAGIC);

        return first_block->body.payload;
    }
    else
    {
        // if splitting causes a splinter, then do not split.
        if (block_size - size < M)
        {
            debug("Splitting will cause splinter, so don't split it.\n");
            free_block->header = ((free_block->header ^ MAGIC) | THIS_BLOCK_ALLOCATED) ^ MAGIC;
            tmp = payload;
            debug("tmp: %ld\n", tmp);
            to_binary(tmp);
            tmp = tmp << 32;
            debug("tmp: %ld\n", tmp);
            to_binary(tmp);
            free_block->header = ((free_block->header ^ MAGIC) | tmp) ^ MAGIC;
            debug("free_block->header: %lu\n", free_block->header ^ MAGIC);
            debug("free_block->prev_footer: %lu\n", free_block->prev_footer ^ MAGIC);
            to_binary(free_block->header ^ MAGIC);

            // remove from free list
            sf_block* prev;
            prev = free_block->body.links.prev;
            prev->body.links.next = free_block->body.links.next;
            free_block->body.links.next = NULL;
            free_block->body.links.prev = NULL;

            // // Turn off the 'prv aloc' bit of the next block in heap.
            // sf_block *next_block = get_next_block((sf_block *)((char *)free_block + 16));
            // if ((unsigned long) next_block <= (unsigned long) sf_mem_end() - 16)
            // {
            //     next_block->header ^= MAGIC;
            //     next_block->header &= PREV_BLOCK_ALLOCATED;
            //     next_block->header ^= MAGIC;
            // }


            return free_block->body.payload;
        }
        else
        {
            debug("Splitting will NOT cause splinter, so split it...\n");
            void *free_block_p = (void *) free_block;
            sf_size_t free_block_size = (free_block->header ^ MAGIC) & BLOCK_SIZE_MASK;
            debug("free_block_size: %d\n", free_block_size);
            to_binary(free_block_size);
            sf_block *allocated_block;
            sf_block *remainder_block;

            allocated_block = (sf_block *) free_block_p;

            // create the header of allocated_block
            sf_size_t free_block_bits = (free_block->header ^ MAGIC) & BITS;
            allocated_block->header = ((sf_header) size | free_block_bits | THIS_BLOCK_ALLOCATED) ^ MAGIC;
            debug("allocated_block->header 1: %ld\n", allocated_block->header ^ MAGIC);
            to_binary(allocated_block->header ^ MAGIC);
            tmp = payload; // payload size
            debug("tmp: %ld\n", tmp);
            to_binary(tmp);
            tmp = tmp << 32;
            debug("tmp: %ld\n", tmp);
            to_binary(tmp);
            allocated_block->header = ((allocated_block->header ^ MAGIC) | tmp) ^ MAGIC;
            debug("allocated_block->header 2: %ld\n", allocated_block->header ^ MAGIC);
            to_binary(allocated_block->header ^ MAGIC);
            allocated_block->prev_footer = free_block->prev_footer; // because allocated_block is the same location in the heap as free_block
            debug("allocated_block->prev_footer: %ld\n", allocated_block->prev_footer ^ MAGIC);
            to_binary(allocated_block->prev_footer ^ MAGIC);

            free_block_p += size;

            // create the header for the remainder block
            sf_size_t remainder_block_size = free_block_size - size;
            remainder_block = (sf_block *) free_block_p;
            remainder_block->header = ((sf_header) remainder_block_size | PREV_BLOCK_ALLOCATED) ^ MAGIC;
            debug("remainder_block->header: %ld\n", remainder_block->header ^ MAGIC);
            to_binary(remainder_block->header ^ MAGIC);
            remainder_block->prev_footer = allocated_block->header; // previous block has no footer

            /* correct the references to remainder_block */

            // if free_block was the first block in its free list, make remainder_block first block in the free list
            if (free_block->body.links.prev == &sf_free_list_heads[index])
            {
                sf_free_list_heads[index].body.links.next = remainder_block;
                sf_free_list_heads[index].body.links.prev = remainder_block;
            }
            // perform below lines for all
            free_block->body.links.prev->body.links.next = remainder_block;
            remainder_block->body.links.prev = free_block->body.links.prev;
            remainder_block->body.links.next = free_block->body.links.next;
            
            // create the footer for remainder_block
            create_footer(remainder_block);

            // check if remainder_block is the last block in the heap. If so, update epilogue->prev_footer
            free_block_p += remainder_block_size;
            void *end = sf_mem_end();
            (void) end;
            if (free_block_p == (sf_mem_end() - 16))
            {
                debug("remainder_block is the last block in the heap.\n");
                sf_block *temp_epilogue;    // temporary epiloque
                temp_epilogue = (sf_block *) free_block_p;
                temp_epilogue->prev_footer = *((sf_footer *) free_block_p);     // update epilogue->prev_footer
                debug("temp_epilogue->prev_footer: %lu\n", ((sf_footer) temp_epilogue->prev_footer) ^ MAGIC);
            }

            
            // relocate the remainder_block if necessary
            // remainder_block_size = (remainder_block->header ^ MAGIC) & BLOCK_SIZE_MASK;
            int res;
            sf_block *temp;
            if ((res = is_relocatable(index, remainder_block_size) == 1))
            {
                debug("Finding the new free list to relocate remainder_block to\n");
                res = find_new_index(remainder_block_size);
                temp = &sf_free_list_heads[res];
                remainder_block->body.links.next = temp->body.links.next;
                remainder_block->body.links.prev = temp;
                temp->body.links.next->body.links.prev = remainder_block;
                temp->body.links.next = remainder_block;

                // remove from free list
                sf_block* prev = free_block->body.links.prev;
                prev->body.links.next = free_block->body.links.next;
                free_block->body.links.next = NULL;
                free_block->body.links.prev = NULL;
            }
            else
            {
                debug("remainder_block remains at index %d\n", index);
            }
            return allocated_block->body.payload;
        }

    }
    
    return NULL;
}

/**
 *  Returns the index in which the remainder block with 
 *  size 'remainder_block_size' should belong in.
 */
int find_new_index(sf_size_t remainder_block_size)
{
    if (remainder_block_size == M)
        return 0;
    else if (remainder_block_size > M && remainder_block_size <= 2*M)
        return 1;
    else if (remainder_block_size > 2*M && remainder_block_size <= 4*M)
        return 2;
    else if (remainder_block_size > 4*M && remainder_block_size <= 8*M)
        return 3;
    else if (remainder_block_size > 8*M && remainder_block_size <= 16*M)
        return 4;
    else if (remainder_block_size > 16*M && remainder_block_size <= 32*M)
        return 5;
    else if (remainder_block_size > 32*M && remainder_block_size <= 64*M)
        return 6;
    else if (remainder_block_size > 64*M && remainder_block_size <= 128*M)
        return 7;
    else if (remainder_block_size > 128*M && remainder_block_size <= 256*M)
        return 8;
    else if (remainder_block_size > 256*M && remainder_block_size <= INT_MAX)
        return 9;
    else
        return -1;
}


/**
 *  Returns 1 if reaminder_block with a block_size of remainder_block_size
 *  needs to be relocated to another free list. Returns 0 if the reaminder_block
 *  should remain in its current free list.
 */
int is_relocatable(int index, sf_size_t remainder_block_size)
{
    int range[2];

    switch(index)
    {
        case 0:
            range[0] = 1 * M;
            range[1] = 1 * M;
            break;
        case 1:
            range[0] = 1 * M;
            range[1] = 2 * M;
            break;
        case 2:
            range[0] = 2 * M;
            range[1] = 4 * M;
            break;
        case 3:
            range[0] = 4 * M;
            range[1] = 8 * M;
            break;
        case 4:
            range[0] = 8 * M;
            range[1] = 16 * M;
            break;
        case 5:
            range[0] = 16 * M;
            range[1] = 32 * M;
            break;
        case 6:
            range[0] = 32 * M;
            range[1] = 64 * M;
            break;
        case 7:
            range[0] = 64 * M;
            range[1] = 128 * M;
            break;
        case 8:
            range[0] = 128 * M;
            range[1] = 256 * M;
            break;
        case 9:
            range[0] = 256 * M;
            range[1] = INT_MAX;
            break;
    }

    if (index == 0 || remainder_block_size == M)
    {
        debug("Relocating is NOT necessary\n");
        return 0;
    }
    if (remainder_block_size > range[0] && remainder_block_size <= range[1])
    {
        debug("Relocating is NOT necessary\n");
        return 0;
    }
    else
    {
        debug("Relocating IS necessary\n");
        return 1;
    }
    
}


sf_size_t calculate_block_size(sf_size_t size)
{   
    sf_size_t total_size = size + 8;    // header
    sf_size_t padding = 0;

    // round to next multiple of 16
    sf_size_t remainder = total_size % 16;
    if (remainder != 0)
    {
        padding = (16 - remainder);
        total_size += padding;
    }

    if (size + padding < 24)   // next, prev, footer
    {
        total_size += 16;
    }

    debug("total_size: %u\n", total_size);
    return total_size;
}

/** 
 *  Adds a new page of memeory to the heap. Coalesces the added page of memory
 *  if the last block on the heap is a free block.
 * 
 *  Returns 0 if adding a new page of memory to the heap is successful.
 *  Return -1 if there is an error calling sf_mem_grow().
 */ 
int add_page()
{
    void *temp;
    sf_block *last_block;
    sf_block *added_block;
    sf_block *epilogue;
    int x;              // 'x' holds temporary values used in pointer arithmetic
    
    int is_allocated = 0;
    temp = sf_mem_end();
    epilogue = (sf_block *)((char *)temp - 16);

    // check if the heap's last block is allocated/free by checking epilogue's 'prev alloc' bit.
    if ((epilogue->header ^ MAGIC) & PREV_BLOCK_ALLOCATED)
        is_allocated = 1;

    /**
     *  If the heap's last block is allocated, add new page of memory.
     *  The heap's epilogue becomes the added block's new header.
     * 
     *  Else, add a new page of memory and immediately coalesce it
     *  with the immediately preceding free block
     */
    if (is_allocated)
    {
        if ((temp = sf_mem_grow()) == NULL)
        {
            debug("Error when calling sf_mem_grow()\n");
            return -1;
        }
        added_block = (sf_block *) ((char *) temp - 16);
        added_block->prev_footer = epilogue->prev_footer;
        added_block->header = (PAGE_SZ | PREV_BLOCK_ALLOCATED) ^ MAGIC;
        create_footer(added_block);

        // create the epilogue
        epilogue = (sf_block *)((char *) added_block + PAGE_SZ);
        epilogue->prev_footer = *((sf_footer *) temp);     // assign epilogue's prev_footer to the address in memory which 'temp' points to.
        epilogue->header = (0x0 | THIS_BLOCK_ALLOCATED) ^ MAGIC;

        // add added_block to free list
        int free_list_index = find_new_index((added_block->header ^ MAGIC) & BLOCK_SIZE_MASK);
        put_in_free_list(added_block, free_list_index);
    }
    else
    {
        if ((temp = sf_mem_grow()) == NULL)
        {
            debug("Error when calling sf_mem_grow()\n");
            return -1;
        }

        // decrement to the beginning address of the added block
        temp -= 16;
        added_block = (sf_block *) temp;

        // decrement to the heap's last block's beginning address.
        x = (((sf_footer) epilogue->prev_footer) ^ MAGIC) & BLOCK_SIZE_MASK;
        debug("x: %d\n", x);
        temp -= x;
        last_block = (sf_block *) temp;

        // Because the last block of the heap is going to change, we remove it from the free list now.
        sf_size_t last_block_size = (last_block->header ^ MAGIC) & BLOCK_SIZE_MASK;
        sf_size_t last_block_size_index = find_new_index(last_block_size);

        // last_block = (sf_block *) ((char *) last_block + 16);       // now points to payload
        remove_from_free_list(last_block, last_block_size_index);

        // coalesce the added block with the heap's last block.
        last_block = coalesce(added_block, 1, 0, 1);
        
        // sf_show_block(last_block);
        
        // create new epilogue
        temp = sf_mem_end() - 16;
        epilogue = (sf_block *) temp;
        epilogue->prev_footer = *(sf_footer *) temp;
        epilogue->header = (0x0 | THIS_BLOCK_ALLOCATED) ^ MAGIC;

        // add block to free list
        last_block_size = (last_block->header ^ MAGIC) & BLOCK_SIZE_MASK;
        int free_list_index = find_new_index(last_block_size);
        put_in_free_list(last_block, free_list_index);
    }

    debug("No errors calling add_page()\n");
    return 0;
}

/**
 *  Coalesces 'added_block' with its immediately preceding free block.
 *  'is_adding_page' is an int that acts like a boolean. If its value is 1, 
 *  then that indicates that this function is being called by the add_page()
 *  function. If its value is 0, then the function is being called by any other function.
 *  
 *  Returns a sf_block pointer to newly coalesced free block's prev_footer.
 */
sf_block *coalesce(sf_block *block, int prev_block_is_free, int next_block_is_free, int is_adding_page)
{
    void *temp;                 // 'temp' is a temporary pointer for pointer arithmetic
    (void) temp;
    sf_block *prev_block;       // free block immediately preceding "added_block"
    sf_block *new_block;
    sf_size_t block_size= ((block->header ^ MAGIC) & BLOCK_SIZE_MASK);
    sf_size_t prev_block_size = 0;
    sf_size_t new_block_size = 0;


    if (is_adding_page)         // if add_page() called coalesce().
    {
        prev_block_size = ((block->prev_footer ^ MAGIC) & BLOCK_SIZE_MASK);     // block points to prev_footer
        debug("prev_block_size: %d\n", prev_block_size);
        prev_block = (sf_block *) ((char *) block - prev_block_size);           // prev_block points to the prev_block's prev_footer

        // calculate the new block size
        new_block_size = prev_block_size + PAGE_SZ;
        debug("new_block_size: %d\n", new_block_size);
        
        // change the immediately prev_block's header to include the new block size
        prev_block->header ^= MAGIC;
        prev_block->header &= BITS;
        prev_block->header |= new_block_size;
        debug("prev_block->header: %ld\n", prev_block->header);
        to_binary(prev_block->header);
        prev_block->header ^= MAGIC;
        
        // create a new footer for the newly size block
        create_footer(prev_block);
        return prev_block;
    }
    sf_block *next_block;
    sf_size_t next_block_size;
    (void) next_block;
    (void) next_block_size;

    if (!prev_block_is_free && !next_block_is_free)
    {
        debug("Why are you calling coalesce()?\n");
        return block;
    }
    else if (prev_block_is_free && !next_block_is_free)
    {
        debug("block address: %p\n", block);
        block = (sf_block *) ((char *)block + 16);  // block now points to payload
        // print_block_stats(block);                   // before: payload; after: still payload

        prev_block = get_prev_block(block);                     // prev_block points to its prev_footer
        debug("prev_block address: %p\n", prev_block);
        
        prev_block = (sf_block *)((char *) prev_block + 16);    // points to payload now
        // print_block_stats(prev_block);                          // before: payload; after: still payload
        
        prev_block = (sf_block *)((char *) prev_block - 16);                // prev_block points to prev_footer
        prev_block_size = ((prev_block->header ^ MAGIC) & BLOCK_SIZE_MASK);

        // remove prev_block from its freelist
        sf_block* prev = prev_block->body.links.prev;
        prev->body.links.next = prev_block->body.links.next;
        prev_block->body.links.next = NULL;
        prev_block->body.links.prev = NULL;        
        
        new_block_size = prev_block_size + block_size;
        debug("new_block_size: %d\n", new_block_size);
        to_binary(new_block_size);

        new_block = prev_block;
        // change the immediately preceding block's header to include the new block size
        new_block->header = (new_block_size | PREV_BLOCK_ALLOCATED) ^ MAGIC;
        new_block->prev_footer = prev_block->prev_footer;   // new_block's prev_footer is whatever next_block->prev_footer is

        // create a new footer for the newly size block
        create_footer(new_block);

        // Turn off 'prv alloc' bit of the next block in the heap.
        sf_block *next_block_in_heap = get_next_block((sf_block *)((char *)new_block + 16));
        next_block_in_heap->header ^= MAGIC;
        next_block_in_heap->header &= ~(PREV_BLOCK_ALLOCATED);
        next_block_in_heap->header ^= MAGIC;

        return new_block;
    }
    else if (!prev_block_is_free && next_block_is_free)
    {
        debug("block address: %p\n", block);

        // block points to payload

        // temp = (void *) block;
        // temp += 16;
        block = (sf_block *) ((char *)block + 16);  // block now points to payload
        // print_block_stats(block);   // before: payload; after: still payload

        // block = (sf_block *)((char *) block + 16);
        next_block = get_next_block(block);     // before: payload; after: prev_footer
        debug("next_block address: %p\n", next_block);
        
        next_block = (sf_block *)((char *) next_block + 16);    // points to payload now
        // print_block_stats(next_block);  // before: payload; after: still payload
        // next_block = (sf_block *)((char *) next_block - 16);

        // temp = (void *) next_block;
        // temp -= 16;
        // next_block = (sf_block *) temp;
    
        next_block = (sf_block *)((char *) next_block - 16);
        next_block_size = ((next_block->header ^ MAGIC) & BLOCK_SIZE_MASK);

        // remove next_block from its freelist
        // remove from free list
        sf_block* prev = next_block->body.links.prev;
        prev->body.links.next = next_block->body.links.next;
        next_block->body.links.next = NULL;
        next_block->body.links.prev = NULL;        
        
        new_block_size = next_block_size + block_size;
        debug("new_block_size: %d\n", new_block_size);
        to_binary(new_block_size);


        new_block = (sf_block *) ((char *) block - 16);
        new_block->header = (new_block_size | PREV_BLOCK_ALLOCATED) ^ MAGIC;
        new_block->prev_footer = block->prev_footer;   // new_block's prev_footer is whatever block->prev_footer is

        // create a new footer for the newly size block
        create_footer(new_block);
        

        return new_block;
    }
    else if (prev_block_is_free && next_block_is_free)
    {
        debug("block address: %p\n", block);

        block = (sf_block *) ((char *)block + 16);  // block now points to payload
        // print_block_stats(block);   // before: payload; after: still payload

        // get prev block
        prev_block = get_prev_block(block);                     // prev_block points to its prev_footer
        debug("prev_block address: %p\n", prev_block);
        
        prev_block = (sf_block *)((char *) prev_block + 16);    // points to payload now
        // print_block_stats(prev_block);                          // before: payload; after: still payload
        
        prev_block = (sf_block *)((char *) prev_block - 16);                // prev_block points to prev_footer
        prev_block_size = ((prev_block->header ^ MAGIC) & BLOCK_SIZE_MASK);

        // remove prev_block from its freelist
        sf_block* prev = prev_block->body.links.prev;
        prev->body.links.next = prev_block->body.links.next;
        prev_block->body.links.next = NULL;
        prev_block->body.links.prev = NULL;

        // get next block
        next_block = get_next_block(block);     // before: payload; after: prev_footer
        debug("next_block address: %p\n", next_block);
        
        next_block = (sf_block *)((char *) next_block + 16);    // points to payload now
        // print_block_stats(next_block);  // before: payload; after: still payload
    
        next_block = (sf_block *)((char *) next_block - 16);
        next_block_size = ((next_block->header ^ MAGIC) & BLOCK_SIZE_MASK);

        // remove next_block from its freelist
        prev = next_block->body.links.prev;
        prev->body.links.next = next_block->body.links.next;
        next_block->body.links.next = NULL;
        next_block->body.links.prev = NULL;        


        new_block_size = prev_block_size + next_block_size + block_size;
        debug("new_block_size: %d\n", new_block_size);
        to_binary(new_block_size);

        // prev_block = get_prev_block(block);
        // prev_block_size = ((prev_block->header ^ MAGIC) & BLOCK_SIZE_MASK);

        // next_block = get_next_block(block);
        // next_block_size = ((next_block->header ^ MAGIC) & BLOCK_SIZE_MASK);

        // new_block_size = next_block_size + block_size;
        // debug("new_block_size: %d\n", new_block_size);

        new_block = prev_block;
        new_block->header = (new_block_size | PREV_BLOCK_ALLOCATED) ^ MAGIC;
        new_block->prev_footer = prev_block->prev_footer;   // new_block's prev_footer is whatever block->prev_footer is
        // create a new footer for the newly size block
        create_footer(new_block);

        return new_block;
    }

    return NULL;
}

/** Return 1 if a block of size 'size' exists in sf_quick_lists.
 *  Return 0 otherwise.
 */
int belongs_in_sf_quick_lists(sf_size_t size)
{
    switch (size)
    {
        case 32:
        case 48:
        case 64:
        case 80:
        case 96:
        case 112:
        case 128:
        case 144:
        case 160:
        case 176:
            return 1;
            break;
        default:
            return 0;
            break;
    }
}

/** 
 *  Puts 'remainder_block' in sf_quick_lists[index]
 */
void put_in_sf_quick_list(sf_block *remainder_block, int index)
{
    sf_block *old_first_block;

    if (sf_quick_lists[index].length == 5)
    {
        flush_sf_quick_list(remainder_block, index);
        sf_quick_lists[index].length++;
        remainder_block->body.links.next = NULL;
        sf_quick_lists[index].first = remainder_block;
        return;
    }
    else if (sf_quick_lists[index].first != NULL)
    {
        // sf_show_block(remainder_block);
        sf_quick_lists[index].length++;
        old_first_block = sf_quick_lists[index].first;
        sf_quick_lists[index].first = remainder_block;
        remainder_block->body.links.next = old_first_block;
        // sf_show_quick_list(index);

        // remainder_block = (sf_block *) ((char*) remainder_block - 16);
        // debug("remainder_block's header: %ld\n", (remainder_block->header ^ MAGIC));
        // to_binary(remainder_block->header ^ MAGIC);
        // remainder_block->header = ((remainder_block->header ^ MAGIC) | IN_QUICK_LIST) ^ MAGIC;  // turn off 'in qklst' bit
        // debug("remainder_block's header: %ld\n", (remainder_block->header ^ MAGIC));
        // to_binary(remainder_block->header ^ MAGIC);


        return;
    }
    else
    {
        // sf_show_block(remainder_block);
        sf_quick_lists[index].length++;
        remainder_block->body.links.next = NULL;
        sf_quick_lists[index].first = remainder_block;
        // sf_show_quick_list(index);
        // remainder_block = (sf_block *) ((char*) remainder_block - 16);
        // debug("remainder_block's header: %ld\n", (remainder_block->header ^ MAGIC));
        // to_binary(remainder_block->header ^ MAGIC);
        // remainder_block->header = ((remainder_block->header ^ MAGIC) | IN_QUICK_LIST) ^ MAGIC;  // turn off 'in qklst' bit
        // debug("remainder_block's header: %ld\n", (remainder_block->header ^ MAGIC));
        // to_binary(remainder_block->header ^ MAGIC);
        return;
    }
}

/** 
 *  When the length of sf_quick_lists[index] is 5, flush it.
 */
void flush_sf_quick_list(sf_block *remainder_block, int index)
{
    sf_block *first_block;
    sf_block *prev_block;
    sf_block *next_block;
    int prev_block_is_free;
    int next_block_is_free;
    // int new_block_free_list_index;

    // first_block = (sf_block *) ((char *) sf_quick_lists[index].first + 16);
    // sf_show_quick_list(index);
    while ((sf_quick_lists[index].first != NULL) && (sf_quick_lists[index].length > 0))
    {
        sf_quick_lists[index].length--;
        first_block = sf_quick_lists[index].first;
        sf_quick_lists[index].first = sf_quick_lists[index].first->body.links.next;
        first_block->body.links.next = NULL;
        // sf_show_quick_list(index);

        debug("first_block->header before: %ld\n", first_block->header ^ MAGIC);
        to_binary(first_block->header ^ MAGIC);
        first_block->header = ((first_block->header ^ MAGIC) & ~(1)) ^ MAGIC;  // turn off 'in qklst' bit
        debug("first_block->header after: %ld\n", first_block->header ^ MAGIC);
        to_binary(first_block->header ^ MAGIC);
        
        first_block = (sf_block *) ((char *) first_block + 16);     // points to payload
        prev_block = get_prev_block(first_block);                   // Now points to prev_footer
        if (((prev_block->header ^ MAGIC) & THIS_BLOCK_ALLOCATED) == 0)
            prev_block_is_free = 1;
        else
            prev_block_is_free = 0;

        next_block = get_next_block(first_block);                   // Now points to prev_footer
        if (((next_block->header ^ MAGIC) & THIS_BLOCK_ALLOCATED) == 0)
            next_block_is_free = 1;
        else
            next_block_is_free = 0;

        // coalesce

        // free first_block
        first_block = (sf_block *) ((char *) first_block - 16);     // points to prev_footer
        first_block->header = (first_block->header ^ MAGIC) & ~(THIS_BLOCK_ALLOCATED);
        first_block->header &= ~(IN_QUICK_LIST);
        first_block->header ^= MAGIC;
        create_footer(first_block);

        // int first_block_free_list_index = find_new_index((first_block->header ^ MAGIC) & BLOCK_SIZE_MASK);
        // put_in_free_list(first_block, first_block_free_list_index);
        
        if (!prev_block_is_free && !next_block_is_free)
        {
            debug("No coalescing necessary\n");
            sf_block *new_block = first_block;      // block points to prev_footer 
            new_block->header = (new_block->header ^ MAGIC) & ~(THIS_BLOCK_ALLOCATED);
            new_block->header ^= MAGIC;
            create_footer(new_block);

            // put in a free list
            int free_list_index = find_new_index((new_block->header ^ MAGIC) & BLOCK_SIZE_MASK);
            put_in_free_list(new_block, free_list_index);
        }
        else
        {
            debug("Coalescing is necessary.\n");
            sf_block *new_block = coalesce(first_block, prev_block_is_free, next_block_is_free, 0);
            int new_block_size = (new_block->header ^ MAGIC) & BLOCK_SIZE_MASK;

            int free_list_index = find_new_index(new_block_size);

            // put new block at the front of the free list
            sf_block *front = sf_free_list_heads[free_list_index].body.links.next;
            if (front == &sf_free_list_heads[free_list_index])
            {
                sf_free_list_heads[free_list_index].body.links.next = new_block;
                new_block->body.links.prev = &sf_free_list_heads[free_list_index];
                new_block->body.links.next = &sf_free_list_heads[free_list_index];
            }
            else
            {
                front->body.links.prev->body.links.next = new_block;
                new_block->body.links.prev = front->body.links.prev;
                new_block->body.links.next = front->body.links.next;
            }
        }
    }
}

void calculate_memory_utilization()
{
    if (sf_mem_start() == sf_mem_end())
        return;
    
    void *p = sf_mem_start();
    sf_block *curr_block = (sf_block *) p;

    double payload_so_far = 0.0;

    while (p != (sf_mem_end() - 16))
    {
        if (p == sf_mem_start())
        {
            debug("The prologue does not count toward memory utilization calculations\n");
        }
        else if (((curr_block->header ^ MAGIC) & IN_QUICK_LIST) != 0)
        {
            debug("Blocks in quick lists do not count toward memory utilization calculations\n");
        }
        else if (((curr_block->header ^ MAGIC) & THIS_BLOCK_ALLOCATED) == 0)
        {
            debug("Block at <%p> is free and will not count towards memory utilization calculations.\n", curr_block);
        }
        else
        {
            debug("Block at <%p> is allocated.\n", curr_block);
            // print_block_stats((sf_block *)((char *) curr_block + 16));
            payload_so_far += (((curr_block->header ^ MAGIC) & PAYLOAD_SIZE_MASK) >> 32);
        }
        debug("aggregate_payload so far: %f\n", payload_so_far);
        p += ((curr_block->header ^ MAGIC) & BLOCK_SIZE_MASK);
        curr_block = (sf_block *) p;
    }

    double heap_size = (sf_mem_end() - sf_mem_start());
    debug("heap_size: %f\n", heap_size);
    memory_utilization = payload_so_far/heap_size;
    debug("memory_utilization: %f\n",memory_utilization);
    debug("peak_memory_utilization: %f\n",peak_memory_utilization);
    if (memory_utilization > peak_memory_utilization)
    {
        peak_memory_utilization = memory_utilization;
    }
    return;
}


void *get_next_block(void *block)
{
    block -= 16;
    debug("block address: %p\n", block);
    sf_size_t block_size = (((sf_block *)block)->header ^ MAGIC) & BLOCK_SIZE_MASK;
    debug("block_size: %d\n", block_size);
    to_binary(block_size);
    
    // sf_block *next_block = block;
    // sf_size_t next_block_size = (next_block->header ^ MAGIC) & BLOCK_SIZE_MASK;
    // next_block = (sf_block *) ((void *) block + next_block_size);
    // return next_block;
    block += block_size;
    sf_block *next_block = (sf_block *) (block);
    
    sf_size_t next_block_size = (next_block->header ^ MAGIC) & BLOCK_SIZE_MASK;

    debug("next_block address: %p\n", next_block);
    debug("next_block_size: %d\n", next_block_size);
    to_binary(next_block_size);

    return next_block;
}

void *get_prev_block(void *block)
{
    block -= 16;
    debug("block address: %p\n", block);
    sf_size_t block_size = (((sf_block *)block)->header ^ MAGIC) & BLOCK_SIZE_MASK;
    debug("block_size: %d\n", block_size);
    to_binary(block_size);
    
    sf_block *tmp = (sf_block *) block;
    sf_size_t prev_block_size = (tmp->prev_footer ^ MAGIC) & BLOCK_SIZE_MASK;
    debug("prev_block_size: %d\n", prev_block_size);

    block -= prev_block_size;
    sf_block *prev_block = (sf_block *) (block);

    debug("prev_block address: %p\n", prev_block);
    debug("prev_block_size: %d\n", prev_block_size);
    to_binary(prev_block_size);


    return prev_block;
}


void to_binary(uint64_t decimal)
{
    int c, k, index;
    uint64_t n;

    char buf2[64];
    (void) buf2;

    n = decimal;
    for (c = 63, index = 0; c >= 0; c--)
    {
        k = n >> c;

        if (k & 1)
            buf2[index++] = '1';
        else
            buf2[index++] = '0';
    }
    buf2[index] = '\0';
    debug("%s\n", buf2);
    return;
}

/** Input: pointer points to block's payload
 *  After calling function: pointer points to block->prev_footer
 *  EDIT: After calling function: pointer points back to the payload.
 * 
 */
void print_block_stats(sf_block *block)
{
    void * tmp = (void *)block;
    tmp -= 16;
    block = (sf_block *) tmp;

    sf_size_t payload_size = ((block->header ^ MAGIC) & PAYLOAD_SIZE_MASK) >> 32;
    sf_size_t block_size = (block->header ^ MAGIC) & BLOCK_SIZE_MASK;
    int alloc = ((block->header ^ MAGIC) & THIS_BLOCK_ALLOCATED) >> 2;
    int prev_alloc = ((block->header ^ MAGIC) & PREV_BLOCK_ALLOCATED) >> 1;;
    int in_quick_list = (block->header ^ MAGIC) & IN_QUICK_LIST;

    printf("Stats for block : %p\n", block);
    printf("payload_size: %d\n", payload_size);
    printf("block_size: %d\n", block_size);
    printf("alloc: %d\n", alloc);
    printf("prev_alloc: %d\n", prev_alloc);
    printf("in_quick_list: %d\n", in_quick_list);   

    block = (sf_block *)((char *) block + 16);
}