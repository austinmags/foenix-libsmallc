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

#ifndef __SMALLOC_H
#define __SMALLOC_H

// allocate memory from the heap
void *smalloc(unsigned long);
// free / release memory back to the heap
void sfree(void*);

//////////////////////////////////////////////////////////////////////////
// some low level calls for diagnostics, testing, and tuning
//////////////////////////////////////////////////////////////////////////

// set the heap memory boundaries and default block allocation size
// WARNING: this will reallocate initial structures and book-keeping values
// DON'T CALL THIS OR ONLY CALL THIS ONCE AT THE BEGINNING OF YOUR PROGRAM
void __smalloc_init(unsigned long bottom, unsigned long top, unsigned long pageSize);

// returns the total number of bytes used by smalloc internals (including smalloc'd data)
// *numBlocks = the number of blocks allocated
// *inBlocks = the number of bytes used, inclusive of headers and smalloc'd memory
unsigned long __smalloc_used(unsigned short *numBlocks, unsigned long *inBlocks);

// returns the available memory
// return value is the unallocated heap size
// *inBlocks is the unallocated memory within all blocks
// *inFree is the memory in previously freed chunks
unsigned long __smalloc_avail(unsigned long *inBlocks, unsigned long *inFree);

// returns the first block -- this is used for diagnostics and testing only
void *__smalloc_first_block(void);

#endif