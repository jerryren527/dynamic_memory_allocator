#include <criterion/criterion.h>
#include <errno.h>
#include <signal.h>
#include "debug.h"
#include "sfmm.h"
#define TEST_TIMEOUT 15

void assert_free_block_count(size_t size, int count);
void assert_quick_list_block_count(size_t size, int count);

float my_abs(float f)
{
    return f <= 0.0 ? -f : f;
}

Test(fragmentation_test_suite, basic_test, .timeout = TEST_TIMEOUT) {
    void *a = sf_malloc(201);
    //sf_show_heap();
    void *b = sf_malloc(202);
    //sf_show_heap();
    void *c = sf_malloc(203);
    //sf_show_heap();
    (void) a;
    (void) b;
    (void) c;

    double internal_fragmentation = sf_internal_fragmentation();
    double peak_utilization = sf_peak_utilization();
    // printf("internal_fragmentation: %f\n",internal_fragmentation);
    // printf("peak_utilization: %f\n",peak_utilization);

    cr_assert(my_abs(internal_fragmentation - 0.901785) < 0.01, "Internal fragmentation too far from correct. Expected [%f], Got [%f]", internal_fragmentation, 0.901785);
    cr_assert(my_abs(peak_utilization - 0.591796) < 0.01, "Internal fragmentation too far from correct. Expected [%f], Got [%f]", peak_utilization, 0.591796);

}

Test(fragmentation_test_suite, basic_test_2, .timeout = TEST_TIMEOUT) {
    void *a = sf_malloc(201);
    //sf_show_heap();
    void *b = sf_malloc(1300);
    //sf_show_heap();
    (void) a;
    (void) b;

    double internal_fragmentation = sf_internal_fragmentation();
    double peak_utilization = sf_peak_utilization();
    // printf("internal_fragmentation: %f\n",internal_fragmentation);
    // printf("peak_utilization: %f\n",peak_utilization);

    cr_assert(my_abs(internal_fragmentation - 0.977213) < 0.01, "Internal fragmentation too far from correct. Expected [%f], Got [%f]", internal_fragmentation, 0.977213);
    cr_assert(my_abs(peak_utilization - 0.732910) < 0.01, "Internal fragmentation too far from correct. Expected [%f], Got [%f]", peak_utilization, 0.732910);
}

Test(fragmentation_test_suite, no_alloc_blocks, .timeout = TEST_TIMEOUT) {
    void *a = sf_malloc(201);
    void *b = sf_malloc(1300);
    //sf_show_heap();
    sf_free(a);
    //sf_show_heap();
    sf_free(b);
    //sf_show_heap();
    (void) b;
    (void) a;

    double internal_fragmentation = sf_internal_fragmentation();
    double peak_utilization = sf_peak_utilization();
    // printf("internal_fragmentation: %f\n",internal_fragmentation);
    // printf("peak_utilization: %f\n",peak_utilization);

    cr_assert(internal_fragmentation == 0.0, "Internal fragmentation is not 0.0. Expected 0.0, Got [%f]", internal_fragmentation);
    cr_assert(my_abs(peak_utilization - 0.732910) < 0.01, "Internal fragmentation too far from correct. Expected [%f], Got [%f]", peak_utilization, 0.732910);
}

Test(fragmentation_test_suite, uninitialized_heap, .timeout = TEST_TIMEOUT) {
    double internal_fragmentation = sf_internal_fragmentation();
    double peak_utilization = sf_peak_utilization();
    // printf("internal_fragmentation: %f\n",internal_fragmentation);
    // printf("peak_utilization: %f\n",peak_utilization);

    cr_assert(internal_fragmentation == 0.0, "Internal fragmentation is not 0.0. Expected 0.0, Got [%f]", internal_fragmentation);
    cr_assert(peak_utilization == 0.0, "Internal fragmentation is not 0.0. Expected 0.0, Got [%f]", peak_utilization);
}

