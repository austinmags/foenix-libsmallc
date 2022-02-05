# smalloc - A simple implementation for retro machines

(This software is licensed under the MIT license. See the source code files for the details.)

Memory allocation is an exercise of performing (simple) pointer arithmetic and 
maintaining a couple kinds of doubly-linked lists. One of the goals of this project
is to write the implementation for readability -- not compactness nor most efficient
outcomes.

Design:
* The basic unit of the heap is a CHUNK.
* A CHUNK includes a header and additional memory space that is at least as large as
  the requestor asked for.
* CHUNKs are transformed to FREED structures when free'd.
* CHUNKs and FREEDs live within the macro unit of a BLOCK
* BLOCKs are in a list and when more memory is needed, a new BLOCK is enqueued at the end.
* BLOCKs are allocated downwards in memory, but CHUNKs are allocated upwards within their BLOCK.

Some ideas for improvement:
* Coalesce adjacent FREED areas - slower free, less memory fragmentation
* Split a FREED area into a CHUNK and remaining FREED - avoid some BLOCK allocations
* Singly-linked list of FREED structures to reduce the minimum size of a CHUNK
* Binning -- blocks with maximum sizes to allow for more compactness, especially
  for smaller memory units. This idea largely comes from my understanding of dlmalloc.

# API

## smalloc - allocates memory from the heap
```c
void *smalloc(unsigned long);
```

The implementation will attempt to ressurrect a previously freed chunk of memory before
allocating more memory out of the heap.

Returns NULL if there is not enough memory to satisify the requested amount.

### Example
```c
#include "libsmallc/smalloc.h"
struct TREENODE {
    void *data;
    struct TREENODE *left;
    struct TREENODE *right;
};

int main() {
    struct TREENODE *node = smalloc(sizeof(struct TREENODE));
    // ...
    sfree(node);
}
```

## sfree - deallocates memory from the heap
```c
void sfree(void*);
```

Deallocates a CHUNK by transforming the memory into a FREED structure
and enqueueing it into the free list in its block. The original memory
is no longer safe to use as it may be reallocated to a subsequent
smalloc caller.

Note: there is little memory protection here. The only sanity check is that the
CHUNK header, which exists in front of the pointer, has a flags value.
The flags value must have the ALLOCD bit set. This at least will avoid problems
with a buggy use that results in a double-sfree of the same memory.

## __smalloc_init - sets the heap boundaries and minimum block size

```c
void __smalloc_init(unsigned long heapTop, unsigned long blockSize, unsigned long heapBottom);
```

Either this should never be called or only called once at the very beginning of the program.

### Example
```c
#include "libsmallc/smalloc.h"
int main() {
    __smalloc_init(0x08ffff, 0x4000, 0x060000);
    // set heap boundary from 0x08ffff down to 0x060000.
    // minimum block allocation size is 0x4000 - 16K bytes
}
```

```heapTop``` must be at least PAGESIZE larger then ```heapBottom```.

## __smalloc_stats - returns statistics about the current state of the heap
```c
unsigned long __smalloc_stats(unsigned short *blocks, unsigned long *freeChunkSz);
```

The return result will be the size of all blocks. This includes unallocated space within
blocks. Essentially this will be the difference from the start of the heap to the current
bottom of the heap. The heap is allowed to grow downwards if necessary, up to ```heapBottom```.

```blocks``` will be the current number of blocks. Blocks are the macro unit of memory
allocation, and are guaranteed to be AT LEAST ```blockSize```. It is possible for blocks
to exceed ```blockSize``` if and only if it is required to satisfy an allocation request and the
boundaries permit the allocation. Example:
```c
// heap is 0x0affff-0x0a0000, allocated in 0x1000 blocks
__smalloc_init(0x0affff, 0x1000, 0x0a0000);
void *bigdata = smalloc(0x2000);
// block will have beeen allocated with size 0x2000 + block header size 
```

```freeChunkSz``` represents the total size of bytes that have been freed and reusable.
This is the sum of ```FREED.header.size``` in each ```FREED``` structure in the free
queue of each ```BLOCK```.
