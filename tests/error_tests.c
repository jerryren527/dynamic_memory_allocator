#include <criterion/criterion.h>
#include <errno.h>
#include <signal.h>
#include "debug.h"
#include "sfmm.h"
#define TEST_TIMEOUT 15

void assert_free_block_count(size_t size, int count);
void assert_quick_list_block_count(size_t size, int count);

Test(error_test_suite, realloc_null, .timeout = TEST_TIMEOUT) {
    sf_errno = 0;
    void *a = sf_realloc(NULL, 25);
    (void) a;
    
    cr_assert(sf_errno == EINVAL, "sf_errno is not set to EINVAL!");
}

Test(error_test_suite, realloc_misaligned, .timeout = TEST_TIMEOUT) {
    sf_errno = 0;
    
    void *a = sf_malloc(1);
    void *a_new = sf_realloc(((void *)a - 8), 5);
    (void) a;
    (void) a_new;
    
    cr_assert(sf_errno == EINVAL, "sf_errno is not set to EINVAL!");
}


Test(error_test_suite, realloc_prev_alloc_0, .timeout = TEST_TIMEOUT) {
    sf_errno = 0;
    
    sf_malloc(178);
    void *a = sf_malloc(2);
    sf_free(a);
    sf_block *a_new = (sf_block *) ((char *)a - 16);
    a_new->header ^= MAGIC;
    a_new->header &= ~(PREV_BLOCK_ALLOCATED);
    a_new->header ^= MAGIC;
    a_new = (sf_block *)((char *)a_new + 16);
    void *b_new = sf_realloc(a_new, 200);
    (void) b_new;    
    
    cr_assert(sf_errno == EINVAL, "sf_errno is not set to EINVAL!");
}

Test(error_test_suite, realloc_prologue, .timeout = TEST_TIMEOUT) {
    sf_errno = 0;
    
    void *a = sf_malloc(1);
    void *b = sf_realloc(sf_mem_start() + 16, 25);
    (void) a;
    (void) b;
    
    cr_assert(sf_errno == EINVAL, "sf_errno is not set to EINVAL!");
}

Test(error_test_suite, realloc_0, .timeout = TEST_TIMEOUT) {
    sf_errno = 0;
    
    void *a = sf_malloc(2);
    (void) a;
    void *a_new = sf_realloc(a, 0);
    (void) a_new;

    assert_quick_list_block_count(0, 1);
    assert_free_block_count(944, 1);
    
    cr_assert(sf_errno == 0, "sf_errno is not set to 0!");
}

Test(error_test_suite, malloc_25000, .timeout = TEST_TIMEOUT) {
    sf_errno = 0;
    
    void *a = sf_malloc(25000);
    (void) a;
    
    cr_assert(a == NULL, "a is not set to NULL!");
    cr_assert(sf_errno == ENOMEM, "sf_errno is not set to ENOMEM!");
}

Test(error_test_suite, realloc_25000, .timeout = TEST_TIMEOUT) {
    sf_errno = 0;
    
    void *a = sf_malloc(1);
    void *b = sf_realloc(a, 25000);
    (void) a;
    (void) b;
    
    cr_assert(b == NULL, "b is not set to NULL!");
    cr_assert(sf_errno == ENOMEM, "sf_errno is not set to ENOMEM!");
}








