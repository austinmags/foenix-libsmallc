/*
Copyright 2022 John A Magdaleno (johnamagdaleno@gmail.com)

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the
following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial
portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.
*/

#include "libsmallc/smalloc.h"
#include <stdlib.h>
#include <stdio.h>

// THESE ARE INTERNALS VVVV

#undef SIZE_T
#define SIZE_T unsigned long

// struct CHUNK is the header of a non-free and free allocation
// within a block
struct CHUNK {
    void *block;
    SIZE_T size;    // total size, inclusive of the CHUNK header
    unsigned flags; 
};

// struct FREED is a struct CHUNK + pointers to maintain
// doubly-linked list of free CHUNKs - the pointers live
// in what was the program data area.
struct FREED {
    struct CHUNK header;
    struct {
        struct FREED* next;
        struct FREED* prev;
    };
};

// struct BLOCK maintains a doubly-linked list of BLOCKs
// It also maintains a pointer, top, to the next memory space to allocate (upward) in the block.
// It maintains a pointer to the head of a doubly-linked list of freed chunks.
// It maintains some size information to make new allocations and reporting easy.
struct BLOCK {
    struct BLOCK* prev;             // previous block, i.e., towards __first_block
    struct BLOCK* next;             // next block, i.e., towards __last_block
    SIZE_T size;                    // size of the entire block, header and actual space
    SIZE_T remaining;               // remaining bytes that can be allocated by smalloc in block
    void*  top;                     // where to start allocating new smalloc requests
    struct FREED* free;             // recycling is important
};
// 24-byte header on 8K default allocation size is 0.3% overhead

// THESE ARE INTERNALS ^^^^^^^

void debug_out(void) {
    unsigned long total, used;
    unsigned short blocks;
    total = __smalloc_used(&blocks, &used);

    unsigned long unallocd, inBlocks, inFree;
    unallocd = __smalloc_avail(&inBlocks, &inFree);

    printf("stats=[%lu %u %lu]     free=[%lu %lu %lu]\n",
        total, blocks, used,
        unallocd, inBlocks, inFree);
}

void print_heap(void) {
    struct BLOCK* block = (struct BLOCK*)__smalloc_first_block();
    printf("__first_block = %p\n", block);
    while(block) {
        printf("\nblock = %p     size=%lu remaining=%lu\n", block, block->size, block->remaining);
        printf("        top=%p free=%p\n", block->top, block->free);

        void *original_top = (void *)block + sizeof(struct BLOCK);
        printf("        start=%p\n", original_top);
        struct CHUNK * chunk = (struct CHUNK *)original_top;
        while((void *)chunk < block->top) {
            printf("        chunk=%p (%lu)\n", chunk, chunk->size);
            void *addr = (void *)chunk;
            //if(!chunk->size) break;
            chunk = (struct CHUNK *)(addr + chunk->size);

        }
        block = block->next;
    }
}

int main(int argc, const char* argv[]) {
    size_t alloc = (1<<16) * 4; // 64k*4 = 256K
    void *b = malloc(alloc);
    void *p = b + alloc - 1;

    // use blocks of 1K -- should allow us to pack allocations
    // and get a bunch of overhead due to many blocks
    __smalloc_init((unsigned long)p, 1 << 10, (unsigned long)b);
    
    // fill the heap
    for(int i=0; i < 512; i++) {
        void *m = smalloc(i+13);
        *((char *)m) = 'I';
        *((int *)(m+1)) = i;
        printf("%p\n", m);
        debug_out();
    }

    // allocate and deallocate the same size -- should result
    // in no incremental heap growth
    for(int i=0; i<1000; i++) {
        void *m = smalloc(128);
        printf("%d: %p     ", i, m);
        debug_out();
        *((char *)m) = 'J';
        *((int *)(m+1)) = i+1;
        sfree(m);
    }

    // allocate something too big
    void *toobig = smalloc(alloc+1);
    // should be 0x0
    printf("toobig = %p\n", toobig);

    void *smaller = smalloc(1025);
    printf("smaller = %p\n", smaller);
    sfree(smaller);
    sfree(smaller); // gasp double-free -- should not mess up the internals
    smaller = smalloc(1025); // this should be the original smaller pointer
    printf("smaller = %p\n", smaller);
    smaller = smalloc(1025); // leaked memory, BUT this should be different address
    printf("smaller = %p\n", smaller);

    // now dump the heap structures
    print_heap();

    return 0;
}