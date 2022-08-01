#include <criterion/criterion.h>
#include <errno.h>
#include <signal.h>
#include "debug.h"
#include "sfmm.h"
#define TEST_TIMEOUT 15

void assert_free_block_count(size_t size, int count);
void assert_quick_list_block_count(size_t size, int count);

/* Testcase tests when reallocation to a smaller size does not cause splinter */
Test(realloc_test_suite, realloc_smaller_no_splinter, .timeout = TEST_TIMEOUT) {

    void *a = sf_malloc(250);
    sf_block *b = (sf_block *) ((char *) sf_malloc(64) - 16);
    sf_block *a_new = (sf_block *) ((char *)sf_realloc(a, 1) - 16);

    (void) a;
    (void) b;
    (void) a_new;
    // sf_show_heap();

    assert_free_block_count(240,1);
    assert_free_block_count(624,1);
    cr_assert((a_new->header ^ MAGIC) & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
    cr_assert(((a_new->header ^ MAGIC) & 0xFFFFFFF0) == 32,
        "Malloc'd block size (%ld) not what was expected (%ld)\n",
        (a_new->header ^ MAGIC) & 0xFFFFFFF0, 32);
    cr_assert((((a_new->header ^ MAGIC) >> 32) & 0xFFFFFFFF) == 1,
        "Malloc'd payload size (%ld) not what was expected (%ld)\n",
        ((a_new->header ^ MAGIC) >> 32) & 0xFFFFFFFF, 1);

    cr_assert((b->header ^ MAGIC) & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
    cr_assert(((b->header ^ MAGIC) & 0xFFFFFFF0) == 80,
        "Malloc'd block size (%ld) not what was expected (%ld)\n",
        (b->header ^ MAGIC) & 0xFFFFFFF0, 80);
    cr_assert((((b->header ^ MAGIC) >> 32) & 0xFFFFFFFF) == 64,
        "Malloc'd payload size (%ld) not what was expected (%ld)\n",
        ((b->header ^ MAGIC) >> 32) & 0xFFFFFFFF, 64);
}

/*  Testcase tests when reallocation to a larger size */
Test(realloc_test_suite, realloc_bigger, .timeout = TEST_TIMEOUT) {

    void *a = sf_malloc(200);
    sf_block *b = (sf_block *) ((char *) sf_malloc(64) - 16);
    void *a_new_p = sf_realloc(a, 500);
    sf_block *a_new = (sf_block *) ((char *) a_new_p - 16);
    // sf_show_heap();

    (void) a;
    (void) b;
    (void) a_new;

    assert_free_block_count(208,1);
    assert_free_block_count(176,1);
    cr_assert((a_new->header ^ MAGIC) & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
    cr_assert(((a_new->header ^ MAGIC) & 0xFFFFFFF0) == 512,
        "Malloc'd block size (%ld) not what was expected (%ld)\n",
        (a_new->header ^ MAGIC) & 0xFFFFFFF0, 512);
    cr_assert((((a_new->header ^ MAGIC) >> 32) & 0xFFFFFFFF) == 500,
        "Malloc'd payload size (%ld) not what was expected (%ld)\n",
        ((a_new->header ^ MAGIC) >> 32) & 0xFFFFFFFF, 500);
}

/*  Testcase tests when reallocation to a smaller size causes a splinter */
Test(realloc_test_suite, realloc_smaller_splinter, .timeout = TEST_TIMEOUT) {

    void *a = sf_malloc(250);
    sf_block *b = (sf_block *) ((char *) sf_malloc(64) - 16);
    sf_block *a_new = (sf_block *) ((char *)sf_realloc(a, 240) - 16);

    (void) a;
    (void) b;
    (void) a_new;
    // sf_show_heap();

    assert_free_block_count(624,1);
    cr_assert((a_new->header ^ MAGIC) & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
    cr_assert(((a_new->header ^ MAGIC) & 0xFFFFFFF0) == 272,
        "Malloc'd block size (%ld) not what was expected (%ld)\n",
        (a_new->header ^ MAGIC) & 0xFFFFFFF0, 272);
    cr_assert((((a_new->header ^ MAGIC) >> 32) & 0xFFFFFFFF) == 240,
        "Malloc'd payload size (%ld) not what was expected (%ld)\n",
        ((a_new->header ^ MAGIC) >> 32) & 0xFFFFFFFF, 240);

}

