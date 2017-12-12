/**************************************************************************
*
* Copyright (C) 2017 Edward Hague
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*********************************************************************/

/*
 (Deeply) Embedded Memory Manager (EMM) for small systems.

 See: http://en.wikipedia.org/wiki/Buddy_memory_allocation

 Designed using "Buddy Memory Allocator" so that internal fragmentation will not slowly ossify available memory, by this
 I mean that some Operating System mallocs() will be unable to allocate a block even though plenty of free memory seems to be available..
 the free memory just has fragmented over time and there is no longer enough CONTIGUOUS memory availabe memory
 to allocate - something I find happens a lot with embedded systems with runtimes extending to years...

 Although some memory is wasted by the fact that only fixed power-of-two blocks are allocatable, the control array
 is 2 bytes * the number of smallest blocks - very efficient.

 Wasted space is about 25% of the allocated memory. 

 */

#include <stdint.h>
#include "emm.h"

// Set up memory allocation using EMM memory manager.

#define MX_Ks  3        // 2^Ks = 8     mimimum allocatable memory size (but includes overhead 1 byte, so this means that the minimum space used is 7 in this case)
#define MX_Ko  10       // Order of memory system. An order of 4 would have orders 0,1,2,3 only. i.e. number of smallest blocks is 2 ^ MX_Ko-1

	// Memory used is 2 ^ (Ks + Ko - 1)
    //  Ks  Ko
    //  3   10  =               4K
	//  3   11  =   2 ^ 13  =   8K
	//  3   12  =   2 ^ 14  =   16K
	//  3   13  =   2 ^ 15  =   32K             

typedef struct _FreeListItem FreeListItem ; 
typedef struct _FreeListItem
{
    FreeListItem    *next;
    uint16_t         orderedAddress;
} FreeListItem;

typedef struct
{
	FreeListItem    *freeList;
} MemoryControlItem;

typedef struct
{
	uint8_t     savedOrder;
	uint8_t     startOfMem[2];				// just to create a reference into memory block
} AllocationUnit;

static uint8_t mainMemory[1UL << (MX_Ks + MX_Ko - 1)];      // If your compiler cannot do this automatically, see calculations above and hardcode in a number.
static MemoryControlItem memoryControlBlock[MX_Ko];

void emm_init(void)
{
	memoryControlBlock[MX_Ko - 1].freeList = (FreeListItem *)mainMemory;
	memoryControlBlock[MX_Ko - 1].freeList->next = NULL;
}


void *emm_malloc( uint16_t size)
{
	uint8_t k, j;
    uint8_t Krequired = 0;
	AllocationUnit *mbf;
	FreeListItem *fli;
	FreeListItem *blockToBeSplit;
    uint16_t ssplit = 1;

	// Using requested size, calculate desired order
	size = size + 1 - 1;        // the 'overhead' (wasted space) in each malloc. One byte is always used by the EMM. 
								// Debug info (fences, kpreserve etc), may use more
								// less one byte for the divide.. i.e. for 8, we want order 3, not 4.
	size >>= MX_Ks;				// divide by the size of the minimum allocation unit
	while (size != 0)
	{
		Krequired++;
		size >>= 1;
	}
	if (Krequired >= MX_Ko)
	{
		return NULL;
	}

    // for multithreaded, lock critical section here

	// Do we have a ready allocated memory block of this order on our free list?
	if (memoryControlBlock[Krequired].freeList != NULL)
	{
		// there is already a spare block for us, unhook it
		mbf = (AllocationUnit *)memoryControlBlock[Krequired].freeList;
		memoryControlBlock[Krequired].freeList = (FreeListItem *)memoryControlBlock[Krequired].freeList->next;

        // for multithreaded, unlock critical section here

		// populate mem block frame
		mbf->savedOrder = Krequired;
		return (void *)mbf->startOfMem;
	}

	// OK, no 'easy' available block found, need to find next largest, and start splitting it down

	for (k = Krequired + 1; k < MX_Ko; k++)
	{
		if (memoryControlBlock[k].freeList != NULL)
		{
			// found the next biggest, start trimming it down..
			// k is our next available memory block.
			blockToBeSplit = memoryControlBlock[k].freeList;
			memoryControlBlock[k].freeList = (FreeListItem *)blockToBeSplit->next;

			// How big is our split block to be? (half the size of the original)
			ssplit <<= (MX_Ks - 1 + k);

			for (j = k - 1; j >= Krequired; j--)
			{
				// iterate backwards, splitting our blocks, adding to freelists, until we get to Krequired, and return that
				fli = (FreeListItem *)((uint8_t *)blockToBeSplit + ssplit);
				fli->orderedAddress = (((uint8_t *)fli - (uint8_t *)mainMemory) >> (MX_Ks + j));
				fli->next = memoryControlBlock[j].freeList;
				memoryControlBlock[j].freeList = fli;
				ssplit >>= 1;

				// deal with the last block, that we allocate to the user
				if (j == Krequired)
				{
					mbf = (AllocationUnit *)blockToBeSplit;
					mbf->savedOrder = Krequired;

                    // for multithreaded, unlock critical section here

                    return (void *)mbf->startOfMem;
				}
			}
		}
	}

	// None found, bugger.. out of memory...
    // for multithreaded, unlock critical section here
	return NULL;
}



void emm_free(void *ptr)
{
	uint8_t *rawMem = (uint8_t *)ptr - 1;
	uint8_t k = ((AllocationUnit *)rawMem)->savedOrder;
	FreeListItem *fli = (FreeListItem *)rawMem;
	FreeListItem *buddyfli;
	FreeListItem **priorfli;
    uint16_t orderedAddress;

    if (ptr == NULL) return;

	orderedAddress = (rawMem - (uint8_t *)mainMemory) >> (MX_Ks + k);

	// remember to do this before the retry!
    // for multithreaded, lock critical section here

RETRY:

	if (memoryControlBlock[k].freeList == NULL)
	{
		// there are no potential blocks to merge with, this is the easy case, just add to free list and get out
		fli->orderedAddress = orderedAddress;
		fli->next = NULL;
		memoryControlBlock[k].freeList = fli;

        // for multithreaded, unlock critical section here

        return;
	}

	// potential merge blocks, this is an iterative process until no buddys found.
	// see if there is a buddy.. calculate the ordered address of the buddy
	orderedAddress ^= 1UL;
	buddyfli = memoryControlBlock[k].freeList;
	priorfli = &memoryControlBlock[k].freeList;

	while (buddyfli != NULL)
	{
		if (buddyfli->orderedAddress == orderedAddress)
		{
			// buddy found, merge and quit

			// pluck the old freeblock from link list
			*priorfli = (FreeListItem *)buddyfli->next;

			// we simply recalculate the raw memory address based on a new 'ordered address' which is the original / 2 (losing the LSB)
			orderedAddress >>= 1;
			k++;
			fli = (FreeListItem *)((uint8_t *)mainMemory + (1UL << (MX_Ks + k)) * orderedAddress);
			goto RETRY;
		}
		priorfli = (FreeListItem **)&buddyfli->next;
		buddyfli = (FreeListItem *)buddyfli->next;
	}

	// no buddy found, just add this released block to free list and get out
	fli->orderedAddress = orderedAddress ^ 1UL;
	fli->next = memoryControlBlock[k].freeList;
	memoryControlBlock[k].freeList = fli;

    // for multithreaded, unlock critical section here
}

