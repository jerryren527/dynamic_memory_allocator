# Dynamic Memory Allocator

This project is a dynamic memory allocator for the x84-64 architecture that performs my own versions of the `malloc`, `realloc`, and `free` functions.

## Introduction

The allocator have the following features:
* Free lists are segregated by size class. Each free list uses the first-fit policy. Each free list also has a set of "quick lists" that hold small blocks segregated by size.
* Free lists are maintined using *last-in-first-out* manner.
* Large blocks will immediately coalesce with adjacent free blocks on `free` calls. However, there will be delayed coalescing of small blocks on `free` calls.
* Boundary tags on free blocks allow for efficient coalescing. Footers are not needed in allocated blocks.
* Blocks will not create splinters when split.
* Allocated blocks are double-memory-row aligned (16-byte) boundaries.
* Block headers and footers are obfuscated to detect heap corruptions and attempts to free blocks not previously obtained via allocation.

This project is course work for a Programming in C course at Stony Brook University.

## Setup

This project is compiled and executed on the Linux Mint 20 Cinnamon operating system.

## Running the Project

Git clone this repository to your local computer running the Linux Mint 20 Cinnamon operating system. Make changes to `src/main.c` and call `sf_malloc`, `sf_realloc`, and `sf_free` functions. Call `sf_show_heap()` to print out the contents of the heap, quick lists, and free lists. Compile the code using the make command, then run the program:

```
// The main function in src/main.c
int main(int argc, char const *argv[]) {
    void *x = sf_malloc(4032);

    (void) x;
    sf_show_heap();

    return EXIT_SUCCESS;
}
```

```
$ make clean all
$ bin/sfmm
Heap start: 0x5575a10c52a0, end: 0x5575a10c62a0, size: 4096
0x5575a10c52a0: [UNUSED  ]                             
0x5575a10c52a8: [PROLOGUE][sz:       32,                al: 1, pal: 0]
0x5575a10c52c8: [USED BLK][sz:     4048, pld:     4032, al: 1, pal: 1]
0x5575a10c6298: [EPILOGUE][sz:        0,                al: 1, pal: 1]

Quick lists:
[0]: 
[1]: 
[2]: 
[3]: 
[4]: 
[5]: 
[6]: 
[7]: 
[8]: 
[9]: 

Free lists:
[0x5575a037e120]: 
[0x5575a037e140]: 
[0x5575a037e160]: 
[0x5575a037e180]: 
[0x5575a037e1a0]: 
[0x5575a037e1c0]: 
[0x5575a037e1e0]: 
[0x5575a037e200]: 
[0x5575a037e220]: 
[0x5575a037e240]: 
```