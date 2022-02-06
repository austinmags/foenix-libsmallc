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

#include "smalloc.h"

/*
  "smalloc" - simple memory allocator for non-mmu machines
 
  The code is original, but concepts were informed from the ideas
  and writings of others. As in almost all things in software,
  we stand on the shoulders of giants.

- Heap starts at specific address, HEAP_TOP, and grows downward.
- Heap grows in BLOCK units. Each block provides memory and
  maintains a list of freed memory out of itself.
- Blocks are PAGESIZE units by default, but a block can
  be allocated to an arbitrarily large size.
- Blocks are a doubly-linked list.
- Freed (released) memory is enqueued as a CHUNK-like structure, FREED,
  that can be searched on subsequent allocations for reuse.
- We try to find a freed chunk of memory first, then a
  block with enough remaining space, and then finally,
  allocate a new block that can accomodate the requested size.
- If we cannot allocate memory within the boundaries (HEAP_BOTTOM, HEAP_TOP)
  we return NULL -- no more memory.
- There is no sbrk.

TODO:
 - Coalesce freed large blocks -- maybe not important?
 - Coalesce freed chunks to form larger available chunks for reallocations
 - Reasonably split freed memory chunks to form new utilized chunks
 - Debug or general memory testing to detect bad memory frees
 - Simplify / reduce the overhead
*/

#ifndef NULL
#define NULL 0
#endif

#undef SIZE_T
#define SIZE_T unsigned long

// The heap starts at HEAP_TOP and grows downward by PAGESIZE, and _never_ past HEAP_BOTTOM
// N.B.: we grow downward because the idea on a smaller memory foot print, the program
// will grow upward, so we attempt to avoid a clash by design without system protection.
static void* HEAP_TOP = (void*)0x07ffff;
static void* HEAP_BOTTOM = (void*)0x050000;
static SIZE_T PAGESIZE = 8192;

#define ALLOCD 1

// struct CHUNK is the header of a non-free and free allocation
// within a block. if CHUNK is at memory M, the "user" (program)
// receives the address to memory, M + sizeof(struct CHUNK).
struct CHUNK {
    void *block;
    SIZE_T size;    // total size, inclusive of the CHUNK header
    unsigned flags; 
};

// struct FREED is a struct CHUNK + pointers to maintain
// a doubly-linked list of free CHUNKs - the pointers live
// in what was the program data area.
// This data structure defines the minimum allocatable
// size of any CHUNK returned by smalloc.
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
    SIZE_T size;                    // size of the entire block: header and space for CHUNKs
    SIZE_T remaining;               // remaining bytes that can be allocated by smalloc in block
    void*  top;                     // where to start allocating new smalloc requests
    struct FREED* free;             // recycling is important
};
// 24-byte header on 8K default allocation size is 0.3% overhead

#define BLOCK_HEADER_SZ            (sizeof(struct BLOCK))

struct BLOCK* __first_block = NULL;
struct BLOCK* __last_block = NULL;

struct FREED* __use_freed_chunk(SIZE_T minSize, SIZE_T maxSize);
struct BLOCK* __block_with_free_space(SIZE_T size);
struct BLOCK* __new_block(SIZE_T requestedSize);

// tuning - OPTIONAL - call at the very beginning, IF at all
void __smalloc_init(SIZE_T top, SIZE_T size, SIZE_T bottom) {
    // guard to prevent a really unfortunate init call
    if(bottom > top || top - bottom < size) return;

    HEAP_TOP = (void*)top;
    PAGESIZE = size;
    HEAP_BOTTOM = (void*)bottom;
    __first_block = NULL;
    __last_block = NULL;
}

// TODO: alignment! (we may generate unaligned pointers!)
void* smalloc(SIZE_T n) {
    // We allocate requested size, n, plus CHUNK header size
    SIZE_T allocSize = n + sizeof(struct CHUNK);

    // The minimum allocation size is actually sizeof(struct FREED) --
    // We need the space for enqueueing the allocated chunk of memory
    // into the "free" list -- when it is freed.
    if(allocSize < sizeof(struct FREED)) allocSize = sizeof(struct FREED);
    // BTW this ensures that we are allocating more than 0 bytes

    // reallocate previously freed memory, if possible
    // we allow reuse of chunks of allocSize, up to allocSize*2
    SIZE_T doubleSize = allocSize << 1;
    struct FREED* freed = __use_freed_chunk(allocSize, doubleSize);
    if(freed) {
        struct CHUNK* chunk = (struct CHUNK *)freed;
        chunk->flags |= ALLOCD;
        return (void*)chunk + sizeof(struct CHUNK);
    }

    // Find a block with enough unused space for the requested size
    struct BLOCK* block = __block_with_free_space(allocSize);
    if(!block) {
        // No existing block exists, so create a new block with enough size.
        block = __new_block(allocSize);
    }

    if(!block) return NULL; // out of memory

    // now allocate out of the block
    struct CHUNK* chunk = block->top;
    block->top += allocSize; // in the block, we grow upwards
    block->remaining -= allocSize;
    chunk->block = block;
    chunk->size = allocSize;
    chunk->flags = (ALLOCD);
    void *touse = (void *)chunk + sizeof(struct CHUNK);
    return touse;
}

// Free a given pointer to smalloc'd space
// 1. Re-establish the struct CHUNK data
// 2. Verify that we are allocated (TODO: add magic word for additional test?)
// 3. Enqueue the chunk into the free-list
// 4. Mark chunk as free (~ALLOCD)
void sfree(void *ptr) {
    struct CHUNK* chunk = ptr - sizeof(struct CHUNK); // reestablish the metadata
    if(!(chunk->flags & ALLOCD)) return; // ruh-roh
    // mark it as free, enqueue it into the free list of the block
    struct BLOCK* block = chunk->block;
    struct FREED* freed = (struct FREED *)chunk;
    freed->prev = NULL;
    freed->next = block->free;
    if(!block->free) {
        block->free = freed;
    } else {
        block->free->prev = freed;
        block->free = freed; 
    }
    freed->header.flags &= ~ALLOCD; // mark it as freed
}

// Find a freed chunk that meets the given size requirements
struct FREED* __use_freed_chunk(SIZE_T minSize, SIZE_T maxSize) {
    struct BLOCK* block = __first_block;
    while(block) {
        struct FREED* freed = block->free;
        while(freed) {
            if(freed->header.size >= minSize && freed->header.size <= maxSize) {
                // found one - dequeue it and return it
                if(freed == block->free) block->free = freed->next;
                if(freed->next) freed->next->prev = freed->prev;
                if(freed->prev) freed->prev->next = freed->next;
                return freed;
            }   
            freed = freed->next;
        }
        block = block->next;
    }
    return NULL;
}

// Find a block with enough free space for the requested size
struct BLOCK* __block_with_free_space(SIZE_T size) {
    struct BLOCK* block = __first_block;
    while(block) {
        if(block->remaining >= size) return block;
        block = block->next;
    }
    return NULL;
}

// Create a new block that minimally satisfies the requested size + overhead
// Use PAGESIZE as a minimum size to ensure we are creating reasonably-sized blocks
struct BLOCK* __new_block(SIZE_T requestedSize) {
    // pick an appropriate block size - this is the actual allocation size of the BLOCK
    // which needs to take into account the minimum block size (PAGESIZE), the requested
    // allocation size, and the size of the block header
    SIZE_T size = requestedSize + BLOCK_HEADER_SZ;
    if(size < PAGESIZE) size = PAGESIZE;

    void *start = __last_block == NULL ? HEAP_TOP : __last_block - 1;
    if(start - size < HEAP_BOTTOM) return NULL; // hit the memory limit

    // <magic>
    // if you have sbrk -- this is where it would be
    // otherwise we "allocate" raw memory space
    struct BLOCK* block = (struct BLOCK*)(start - size); // grow downward
    // </magic>

    // book-keeping to make insertions and CHUNK allocations easy
    block->free = NULL;
    block->prev = __last_block;
    block->next = NULL;
    block->size = size;
    block->remaining = block->size - BLOCK_HEADER_SZ;
    block->top = (void *)block + BLOCK_HEADER_SZ; // upwards inside the block
    if(__first_block == NULL) {
        __first_block = block;
        __last_block = block;
    } else {
        __last_block->next = block;
        __last_block = block;
    }
    return block;
}

// Returns the total space taken by smalloc structures - utilized or not
// blocks will hold the count of blocks allocated
SIZE_T __smalloc_used(unsigned short *numBlocks, unsigned long *inBlocks) {
    SIZE_T size = 0;
    struct BLOCK* block = __first_block;
    *numBlocks = 0;
    *inBlocks = 0;
    while(block) {
        (*numBlocks)++;
        *inBlocks += block->size - block->remaining;
        size += block->size;
        block = block->next;
    }
    return size;
}

SIZE_T __smalloc_avail(SIZE_T *inBlocks, SIZE_T *inFree) {
    SIZE_T unallocdHeap = __last_block ? 
        (void*)__last_block - HEAP_BOTTOM : 
        HEAP_TOP - HEAP_BOTTOM;
    
    *inBlocks = 0;
    *inFree = 0;

    struct BLOCK* block = __first_block;
    while(block) {
        *inBlocks += block->remaining;
        struct FREED* freed = block->free;
        while(freed) {
            *inFree += freed->header.size;
            freed = freed->next;
        }
        block = block->next;
    }
    return unallocdHeap;
}

void *__smalloc_first_block(void) {
    return (void *)__first_block;
}
