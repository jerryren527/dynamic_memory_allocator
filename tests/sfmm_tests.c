#include <criterion/criterion.h>
#include <errno.h>
#include <signal.h>
#include "debug.h"
#include "sfmm.h"
#define TEST_TIMEOUT 15

/*
 * Assert the total number of free blocks of a specified size.
 * If size == 0, then assert the total number of all free blocks.
 */
void assert_free_block_count(size_t size, int count) {
    int cnt = 0;
    for(int i = 0; i < NUM_FREE_LISTS; i++) {
	sf_block *bp = sf_free_list_heads[i].body.links.next;
	while(bp != &sf_free_list_heads[i]) {
	    if(size == 0 || size == ((bp->header ^ MAGIC) & 0xfffffff0))
		cnt++;
	    bp = bp->body.links.next;
	}
    }
    if(size == 0) {
	cr_assert_eq(cnt, count, "Wrong number of free blocks (exp=%d, found=%d)",
		     count, cnt);
    } else {
	cr_assert_eq(cnt, count, "Wrong number of free blocks of size %ld (exp=%d, found=%d)",
		     size, count, cnt);
    }
}

/*
 * Assert the total number of quick list blocks of a specified size.
 * If size == 0, then assert the total number of all quick list blocks.
 */
void assert_quick_list_block_count(size_t size, int count) {
    int cnt = 0;
    for(int i = 0; i < NUM_QUICK_LISTS; i++) {
	sf_block *bp = sf_quick_lists[i].first;
	while(bp != NULL) {
	    if(size == 0 || size == ((bp->header ^ MAGIC) & 0xfffffff0)) {
		cnt++;
		if(size != 0) {
		    // Check that the block is in the correct list for its size.
		    int index = (size - 32) >> 4;
		    cr_assert_eq(index, i, "Block %p (size %ld) is in wrong quick list for its size "
				 "(expected %d, was %d)",
				 &bp->header, (bp->header ^ MAGIC) & 0xfffffff0, index, i);
		}
	    }
	    bp = bp->body.links.next;
	}
    }
    if(size == 0) {
	cr_assert_eq(cnt, count, "Wrong number of quick list blocks (exp=%d, found=%d)",
		     count, cnt);
    } else {
	cr_assert_eq(cnt, count, "Wrong number of quick list blocks of size %ld (exp=%d, found=%d)",
		     size, count, cnt);
    }
}

Test(sfmm_basecode_suite, malloc_an_int, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	size_t sz = sizeof(int);
	int *x = sf_malloc(sz);

	cr_assert_not_null(x, "x is NULL!");

	*x = 4;
	// sf_show_heap();

	cr_assert(*x == 4, "sf_malloc failed to give proper space for an int!");
	sf_block *bp = (sf_block *)((char *)x - 16);
	cr_assert((((bp->header ^ MAGIC) >> 32) & 0xffffffff) == sz,
		  "Malloc'ed block payload size (%ld) not what was expected (%ld)!",
		  (((bp->header ^ MAGIC) >> 32) & 0xffffffff), sz);

	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 1);
	assert_free_block_count(944, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
	cr_assert(sf_mem_start() + PAGE_SZ == sf_mem_end(), "Allocated more than necessary!");
}

Test(sfmm_basecode_suite, malloc_four_pages, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;

	void *x = sf_malloc(4032);
	cr_assert_not_null(x, "x is NULL!");
	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 0);
	cr_assert(sf_errno == 0, "sf_errno is not 0!");
}

Test(sfmm_basecode_suite, malloc_too_large, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	void *x = sf_malloc(98304);
	//sf_show_heap();

	cr_assert_null(x, "x is not NULL!");
	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 1);
	assert_free_block_count(24528, 1);
	cr_assert(sf_errno == ENOMEM, "sf_errno is not ENOMEM!");
}

Test(sfmm_basecode_suite, free_quick, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	size_t sz_x = 8, sz_y = 32, sz_z = 1;
	/* void *x = */ sf_malloc(sz_x);
	void *y = sf_malloc(sz_y);
	/* void *z = */ sf_malloc(sz_z);

	sf_free(y);
	//sf_show_heap();

	assert_quick_list_block_count(0, 1);
	assert_quick_list_block_count(48, 1);
	assert_free_block_count(0, 1);
	assert_free_block_count(864, 1);
	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sfmm_basecode_suite, free_no_coalesce, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	size_t sz_x = 8, sz_y = 200, sz_z = 1;
	/* void *x = */ sf_malloc(sz_x);
	void *y = sf_malloc(sz_y);
	/* void *z = */ sf_malloc(sz_z);

	sf_free(y);
	//sf_show_heap();

	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 2);
	assert_free_block_count(208, 1);
	assert_free_block_count(704, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sfmm_basecode_suite, free_coalesce, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	size_t sz_w = 8, sz_x = 200, sz_y = 300, sz_z = 4;
	/* void *w = */ sf_malloc(sz_w);
	void *x = sf_malloc(sz_x);
	void *y = sf_malloc(sz_y);
	/* void *z = */ sf_malloc(sz_z);

	sf_free(y);
	sf_free(x);
	//sf_show_heap();

	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 2);
	assert_free_block_count(384, 1);
	assert_free_block_count(528, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sfmm_basecode_suite, freelist, .timeout = TEST_TIMEOUT) {
        size_t sz_u = 200, sz_v = 150, sz_w = 50, sz_x = 150, sz_y = 200, sz_z = 250;
	void *u = sf_malloc(sz_u);
	/* void *v = */ sf_malloc(sz_v);
	void *w = sf_malloc(sz_w);
	/* void *x = */ sf_malloc(sz_x);
	void *y = sf_malloc(sz_y);
	/* void *z = */ sf_malloc(sz_z);
	//sf_show_heap();

	sf_free(u);
	//sf_show_heap();
	sf_free(w);
	//sf_show_heap();
	sf_free(y);
	//sf_show_heap();

	assert_quick_list_block_count(0, 1);
	assert_free_block_count(0, 3);
	assert_free_block_count(208, 2);
	assert_free_block_count(928, 1);

	// First block in list should be the most recently freed block not in quick list.
	int i = 3;
	sf_block *bp = sf_free_list_heads[i].body.links.next;
	cr_assert_eq(&bp->header, (char *)y - 8,
		     "Wrong first block in free list %d: (found=%p, exp=%p)",
                     i, &bp->header, (char *)y - 8);
}

Test(sfmm_basecode_suite, realloc_larger_block, .timeout = TEST_TIMEOUT) {
        size_t sz_x = sizeof(int), sz_y = 10, sz_x1 = sizeof(int) * 20;
	void *x = sf_malloc(sz_x);
	/* void *y = */ sf_malloc(sz_y);
	x = sf_realloc(x, sz_x1);
	//sf_show_heap();

	cr_assert_not_null(x, "x is NULL!");
	sf_block *bp = (sf_block *)((char *)x - 16);
	cr_assert((bp->header ^ MAGIC) & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert(((bp->header ^ MAGIC) & 0xfffffff0) == 96,
		  "Realloc'ed block size (%ld) not what was expected (%ld)!",
		  (bp->header ^ MAGIC) & 0xfffffff0, 96);
	cr_assert((((bp->header ^ MAGIC) >> 32) & 0xffffffff) == sz_x1,
		  "Realloc'ed block payload size (%ld) not what was expected (%ld)!",
		  (((bp->header ^ MAGIC) >> 32) & 0xffffffff), sz_x1);

	assert_quick_list_block_count(0, 1);
	assert_quick_list_block_count(32, 1);
	assert_free_block_count(0, 1);
	assert_free_block_count(816, 1);
}

Test(sfmm_basecode_suite, realloc_smaller_block_splinter, .timeout = TEST_TIMEOUT) {
        size_t sz_x = sizeof(int) * 20, sz_y = sizeof(int) * 16;
	void *x = sf_malloc(sz_x);
	void *y = sf_realloc(x, sz_y);
	//sf_show_heap();

	cr_assert_not_null(y, "y is NULL!");
	cr_assert(x == y, "Payload addresses are different!");

	sf_block *bp = (sf_block *)((char *)x - 16);
	cr_assert((bp->header ^ MAGIC) & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert(((bp->header ^ MAGIC) & 0xfffffff0) == 96,
		  "Realloc'ed block size (%ld) not what was expected (%ld)!",
		  (bp->header ^ MAGIC) & 0xfffffff0, 96);
	cr_assert((((bp->header ^ MAGIC) >> 32) & 0xffffffff) == sz_y,
		  "Realloc'ed block payload size (%ld) not what was expected (%ld)!",
		  (((bp->header ^ MAGIC) >> 32) & 0xffffffff), sz_y);

	// There should be only one free block.
	assert_quick_list_block_count(0, 0);
	assert_free_block_count(0, 1);
	assert_free_block_count(880, 1);
}

Test(sfmm_basecode_suite, realloc_smaller_block_free_block, .timeout = TEST_TIMEOUT) {
        size_t sz_x = sizeof(double) * 8, sz_y = sizeof(int);
	void *x = sf_malloc(sz_x);
	void *y = sf_realloc(x, sz_y);
	//sf_show_heap();

	cr_assert_not_null(y, "y is NULL!");

	sf_block *bp = (sf_block *)((char *)x - 16);
	cr_assert((bp->header ^ MAGIC) & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert(((bp->header ^ MAGIC) & 0xfffffff0) == 32,
		  "Realloc'ed block size (%ld) not what was expected (%ld)!",
		  (bp->header ^ MAGIC) & 0xfffffff0, 32);
	cr_assert((((bp->header ^ MAGIC) >> 32) & 0xffffffff) == sz_y,
		  "Realloc'ed block payload size (%ld) not what was expected (%ld)!",
		  (((bp->header ^ MAGIC) >> 32) & 0xffffffff), sz_y);

	// After realloc'ing x, we can return a block of size 48
	// to the freelist.  This block will go into the main freelist and be coalesced.
	// Note that we don't put split blocks into the quick lists because their sizes are not sizes
	// that were requested by the client, so they are not very likely to satisfy a new request.
	assert_quick_list_block_count(0, 0);	
	assert_free_block_count(0, 1);
	assert_free_block_count(944, 1);
}

//############################################
//STUDENT UNIT TESTS SHOULD BE WRITTEN BELOW
//DO NOT DELETE THESE COMMENTS
//############################################

/**
 *  Tests flush_sf_quick_list(). The quick list should be flushed. Each block 
 * 	is coalesced, if necessary, and added to a free list.
 */
Test(sfmm_student_suite, student_test_1, .timeout = TEST_TIMEOUT) {
	void *a = sf_malloc(1);
    // sf_show_heap();
    void *b = sf_malloc(2);
    // sf_show_heap();
    void *c = sf_malloc(3);
    // sf_show_heap();
    void *d = sf_malloc(4);
    // sf_show_heap();
    void *e = sf_malloc(5);
    // sf_show_heap();
    void *f = sf_malloc(6);

    sf_free(a);
    sf_free(b);
    sf_free(c);
    sf_free(d);
    sf_free(e);
    sf_free(f);

	assert_quick_list_block_count(32, 1);
	assert_free_block_count(160, 1);
	assert_free_block_count(784, 1);
}

/**	Tests add_page(). If the last block of the heap is allocated when calling
 * 	add_page(), a new block of size PAGE_SZ is added, and a new epilogue is created.
 */
Test(sfmm_student_suite, student_test_2, .timeout = TEST_TIMEOUT) {
	void *a = sf_malloc(952);
    void *b = sf_malloc(176);
	(void) a;
	(void) b;
	// sf_show_heap();

	assert_free_block_count(832, 1);
	sf_block *ap = (sf_block *)((char *)a - 16);
	cr_assert((ap->header ^ MAGIC) & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert(((ap->header ^ MAGIC) & 0xfffffff0) == 976,
		  "Malloc'ed block size (%ld) not what was expected (%ld)!",
		  (ap->header ^ MAGIC) & 0xfffffff0, 976);
	cr_assert((((ap->header ^ MAGIC) >> 32) & 0xffffffff) == 952,
		  "Malloc'ed block payload size (%ld) not what was expected (%ld)!",
		  (((ap->header ^ MAGIC) >> 32) & 0xffffffff), 952);

	sf_block *bp = (sf_block *)((char *)b - 16);
	cr_assert((bp->header ^ MAGIC) & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert(((bp->header ^ MAGIC) & 0xfffffff0) == 192,
		  "Malloc'ed block size (%ld) not what was expected (%ld)!",
		  (bp->header ^ MAGIC) & 0xfffffff0, 192);
	cr_assert((((bp->header ^ MAGIC) >> 32) & 0xffffffff) == 176,
		  "Malloc'ed block payload size (%ld) not what was expected (%ld)!",
		  (((bp->header ^ MAGIC) >> 32) & 0xffffffff), 176);
}

/**	Tests coalescing a block whose immediately preceding block and immediately
 * 	next block are free.
 */
Test(sfmm_student_suite, student_test_3, .timeout = TEST_TIMEOUT) {
	void *b = sf_malloc(176);
    void *c = sf_malloc(176);
    void *d = sf_malloc(176);
	sf_malloc(176);

	sf_free(b);
    sf_free(d);
	// sf_show_heap();
    sf_free(c);
    // sf_show_heap();

	assert_free_block_count(208, 1);
	assert_free_block_count(576, 1);
	assert_quick_list_block_count(0, 0);
}

/**	Tests coalescing a block whose immediately preceding block is free 
 * 	and next block is allocated.
 */
Test(sfmm_student_suite, student_test_4, .timeout = TEST_TIMEOUT) {
	void *b = sf_malloc(176);
    void *c = sf_malloc(176);
    sf_malloc(176);
    
	sf_free(b);
    sf_free(c);
    // sf_show_heap();

	assert_free_block_count(384, 1);
	assert_free_block_count(400, 1);
	assert_quick_list_block_count(0, 0);
}

/**	Tests adding multiple pages of memory.
 * 
 */
Test(sfmm_student_suite, student_test_5, .timeout = TEST_TIMEOUT) {
    sf_errno = 0;

    void *x = sf_malloc(9000);
	// sf_show_heap();
    cr_assert_not_null(x, "x is NULL!");
    assert_quick_list_block_count(0, 0);
    assert_free_block_count(160, 1);
    cr_assert(sf_errno == 0, "sf_errno is not zero!");
}
