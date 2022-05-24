/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/

/* single word (4) or double word (8) alignment */
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int*)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

static void *heap_listp;
static void *last_allocated;
static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void *find_fit(size_t size);
static void place(void *bp, size_t size);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
	if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
		return -1;
	PUT(heap_listp, 0);								// alignment padding
	PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));	// prologue header
	PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));	// prologue footer
	PUT(heap_listp + (3*WSIZE), PACK(0, 1));		// epilogue header
	heap_listp += (2*WSIZE);
	last_allocated = heap_listp;	// init for next fit

	if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
		return -1;
    return 0;
}

static void *extend_heap(size_t words)
{
	void *bp;
	size_t size;

	size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
	if ((long)(bp = mem_sbrk(size)) == -1)
		return NULL;
	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size, 0));
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));

	return coalesce(bp);
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);
	int extendsize;
	void *bp;
	if (size == 0)
		return NULL;
	if ((bp = find_fit(newsize)) != NULL) {
		place(bp, newsize);
		return bp;
	}

	extendsize = MAX(newsize, CHUNKSIZE);
	if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
		return NULL;
	place(bp, newsize);
	last_allocated = bp;
	return bp;
}
/*
// first-fit
static void *find_fit(size_t size)
{
	char *bp = heap_listp;
	while(GET_SIZE(HDRP(bp)) > 0) {
		if (!GET_ALLOC(HDRP(bp)) && (size <= GET_SIZE(HDRP(bp))))
			return bp;
		bp = NEXT_BLKP(bp);
	}
	return NULL;
}
*/
// next-fit
static void *find_fit(size_t size)
{
	void *bp = last_allocated;
	// find fit from last allocated address
	while(GET_SIZE(HDRP(bp)) > 0) {
		if (!GET_ALLOC(HDRP(bp)) && (size <= GET_SIZE(HDRP(bp)))) {
			last_allocated = bp;
			return bp;
		}
		bp = NEXT_BLKP(bp);
	}
	// cannot find fit -> find from first address to last allocated address
	bp = heap_listp;
	while(bp < last_allocated) {
		if (!GET_ALLOC(HDRP(bp)) && (size <= GET_SIZE(HDRP(bp)))) {
			last_allocated = bp;
			return bp;
		}
		bp = NEXT_BLKP(bp);
	}
	return NULL;
}

static void place(void *bp, size_t size)
{
	size_t blocksize = GET_SIZE(HDRP(bp));
	if (blocksize - size < (2*DSIZE)) {
		PUT(HDRP(bp), PACK(blocksize, 1));
		PUT(FTRP(bp), PACK(blocksize, 1));
	}
	else {
		PUT(HDRP(bp), PACK(size, 1));
		PUT(FTRP(bp), PACK(size, 1));
		bp = NEXT_BLKP(bp);
		PUT(HDRP(bp), PACK(blocksize - size, 0));
		PUT(FTRP(bp), PACK(blocksize - size, 0));
		coalesce(bp);
	}
}
/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
	size_t size = GET_SIZE(HDRP(ptr));

	PUT(HDRP(ptr), PACK(size, 0));
	PUT(FTRP(ptr), PACK(size, 0));
	coalesce(ptr);
}

static void *coalesce(void *bp)
{
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));

	if (prev_alloc && next_alloc) return bp;

	else if (prev_alloc && !next_alloc) {
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
	}

	else if (!prev_alloc && next_alloc) {
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		PUT(FTRP(bp), PACK(size, 0));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
	}

	else {
		size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
			GET_SIZE(FTRP(NEXT_BLKP(bp)));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
	}
	last_allocated = bp;
	return bp;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
	void *oldptr = ptr;
    void *newptr;
    size_t copySize;
	size_t oldsize = GET_SIZE(HDRP(oldptr));
	size_t newsize = ALIGN(size + SIZE_T_SIZE) + 2*DSIZE;
	// when newsize is smaller than current size
	if (newsize <= oldsize) {
		// if the difference between newsize and oldsize is bigger than minimum block size
		// then free remainder
		if (oldsize - newsize > 0) {
			PUT(HDRP(oldptr), PACK(newsize, 1));
			PUT(FTRP(oldptr), PACK(newsize, 1));
			PUT(HDRP(NEXT_BLKP(oldptr)), PACK(oldsize-newsize, 0));
			PUT(FTRP(NEXT_BLKP(oldptr)), PACK(oldsize-newsize, 0));
			coalesce(HDRP(NEXT_BLKP(oldptr)));
		}
		last_allocated = oldptr;
		return oldptr;
	}

	int next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(oldptr)));
	size_t blocksize = oldsize + GET_SIZE(HDRP(NEXT_BLKP(oldptr)));

	if (!next_alloc && blocksize > newsize) {
		if (blocksize - newsize < 2*DSIZE) {
			PUT(HDRP(oldptr), PACK(blocksize, 1));
			PUT(FTRP(oldptr), PACK(blocksize, 1));
		}
		else {
			PUT(HDRP(oldptr), PACK(newsize, 1));
			PUT(FTRP(oldptr), PACK(newsize, 1));
			PUT(HDRP(NEXT_BLKP(oldptr)), PACK(blocksize - newsize, 0));
			PUT(FTRP(NEXT_BLKP(oldptr)), PACK(blocksize - newsize, 0));
			coalesce(NEXT_BLKP(oldptr));
		}
		last_allocated = oldptr;
		return oldptr;
	}

	newptr = mm_malloc(size);
	if (newptr == NULL)
		return NULL;
	copySize = GET_SIZE(HDRP(oldptr));
	if (size < copySize)
		copySize = size;
	memcpy(newptr, oldptr, copySize);
	mm_free(oldptr);
	last_allocated = newptr;
	return newptr;
}
