#include <criterion/criterion.h>
#include <errno.h>
#include <signal.h>
#include "debug.h"
#include "sfmm.h"
#define TEST_TIMEOUT 15

void assert_free_block_count(size_t size, int count);
void assert_quick_list_block_count(size_t size, int count);

/*  The 'prv alloc' bit of the allocated block after the free block should be 0 */
Test(bits_test_suite, free_no_coalesce, .timeout = TEST_TIMEOUT) {
    sf_errno = 0;

    sf_malloc(4);
    void *a = sf_malloc(200);
    sf_block *b = (sf_block *) ((char *)sf_malloc(5) - 16);
    //sf_show_heap();
    sf_free(a);
    //sf_show_heap();

    cr_assert(((b->header ^ MAGIC) & PREV_BLOCK_ALLOCATED) == 0, "The malloc'd block after the free block should be 0.");
}

/*  The 'prev alloc' bit of the allocated block after the free block should be 0.
    This testcase tests coalesce() when the prev_block is free and the next_block is allocated.
 */
Test(bits_test_suite, coalesce_prev_block_free, .timeout = TEST_TIMEOUT) {
    void *a = sf_malloc(200);
    void *b = sf_malloc(201);
    sf_block *c = (sf_block *)((char *)sf_malloc(202) - 16);
    sf_malloc(203);
    //sf_show_heap();
    (void) a;
    (void) b;
    sf_free(a);
    sf_free(b);
    //sf_show_heap();

    cr_assert(((c->header ^ MAGIC) & PREV_BLOCK_ALLOCATED) == 0, "The malloc'd block after the free block should be 0.");
}


/*  The 'prev alloc' bit of the allocated block after the free block should be 0.
    This testcase tests coalesce() when the prev_block is allocated and the next_block is free.
 */
Test(bits_test_suite, coalesce_next_block_free, .timeout = TEST_TIMEOUT) {
    void *a = sf_malloc(200);
    void *b = sf_malloc(201);
    void *c = sf_malloc(202);
    sf_block *d = (sf_block *) ((char *) sf_malloc(203) - 16);
    //sf_show_heap();
    (void) a;
    (void) b;
    (void) c;
    (void) d;
    sf_free(c);
    //sf_show_heap();
    sf_free(b);
    //sf_show_heap();

    cr_assert(((d->header ^ MAGIC) & PREV_BLOCK_ALLOCATED) == 0, "The malloc'd block after the free block should be 0.");
}


/*  The 'prev alloc' bit of the allocated block after the free block should be 0.
    This testcase tests coalesce() when the prev_block is free AND the next_block is free.
 */
Test(bits_test_suite, coalesce_prev_and_next_block_free, .timeout = TEST_TIMEOUT) {
    void *a = sf_malloc(200);
    void *b = sf_malloc(201);
    void *c = sf_malloc(202);
    sf_block *d = (sf_block *)((char *)sf_malloc(203) - 16);
    void *e = sf_malloc(203);
    //sf_show_heap();
    (void) a;
    (void) b;
    (void) c;
    (void) d;
    (void) e;
    sf_free(a);
    //sf_show_heap();
    sf_free(c);
    //sf_show_heap();
    sf_free(b);
    //sf_show_heap();

    cr_assert(((d->header ^ MAGIC) & PREV_BLOCK_ALLOCATED) == 0, "The malloc'd block after the free block should be 0.");
}



/*  This testcase will test that a formally free block that is now allocated will turn on the 
    'prev alloc' bit of the next block in the heap.    
*/
Test(bits_test_suite, malloc_after_being_freed, .timeout = TEST_TIMEOUT) {
    void *a = sf_malloc(200);
    sf_block *b = (sf_block *)((char *)sf_malloc(201) - 16);
    void *c = sf_malloc(202);
    // void *d = sf_malloc(203);
    // void *e = sf_malloc(203);
    //sf_show_heap();
    (void) a;
    (void) b;
    (void) c;
    // (void) d;
    // (void) e;
    sf_free(a);
    //sf_show_heap();
    sf_malloc(200);
    //sf_show_heap();

    cr_assert(((b->header ^ MAGIC) & PREV_BLOCK_ALLOCATED), "The malloc'd block after the free block should be 1.");
}