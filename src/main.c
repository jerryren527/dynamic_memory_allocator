#include <stdio.h>
#include "sfmm.h"
#include "helper.h"
#include "debug.h"

int main(int argc, char const *argv[]) {
    void *x = sf_malloc(4032);

    (void) x;
    sf_show_heap();

    return EXIT_SUCCESS;
}
