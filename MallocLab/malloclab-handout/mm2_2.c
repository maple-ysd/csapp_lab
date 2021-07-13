/*
 * mm2_2.c - explicit double linked list(free blocks) + first-fit malloc package.
 *	free block sort according to address, which means always put the free block in position, 
 *	before which the addresses are less than it and after which address bigger than it
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "maple-ysd",
    /* First member's email address */
    "xxx@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

// #define DEBUG
/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// Basic constants and macros
#define WSIZE 4						// single word size(bytes)
#define DSIZE 8						// double word size(bytes)
#define CHUNKSIZE (1 << 12) 		// extend heap by this amount(bytes) the page size in bytes is 4K

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */ 
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Given block bp, compute address of next and previous free blocks */
#define PREV_NODE(bp) ((char*)(bp))
#define NEXT_NODE(bp) ((char*)(bp) + WSIZE)

static char *heap_listp = NULL;
static char *root = NULL;	// performs as a dummy node of double linked list of free blocks

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

static void remove_from_emptyList(void *bp);
static void insert_to_emptyList(void *bp);
int mm_check(char *function);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    mem_init();
    // Create the initial empty heap
    if ((heap_listp = mem_sbrk(6 * WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);				// Alignment paddin
    PUT(heap_listp + (1 * WSIZE), 0);	 
    PUT(heap_listp + (2 * WSIZE), 0);
    PUT(heap_listp + (3 * WSIZE), PACK(DSIZE, 1));	// Prologue header
    PUT(heap_listp + (4 * WSIZE), PACK(DSIZE, 1));	// Prologue footer
    PUT(heap_listp + (5 * WSIZE), PACK(0, 1));	// Epilogue header
    root = heap_listp + WSIZE;
    heap_listp += (4 * WSIZE);

    // Extend the empty heap with a free block of CHUNKSIZE bytes
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    
    #ifdef DEBUG
        mm_check(__FUNCTION__);
    #endif
    return 0;
}

/* 
 * extend_heap - Extend the heap 
 */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size; 
    
    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((bp = mem_sbrk(size)) == (void *)-1)
        return NULL;
    
    /* initialize free block prev/next, header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(PREV_NODE(bp), 0);
    PUT(NEXT_NODE(bp), 0);
    PUT(HDRP(NEXT_BLKP(bp)),  PACK(0, 1));
    /* Coalesce if the previous block was free */
    return coalesce(bp);    
}
/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    char *bp;
    size_t extendsize;
    // ignore spurious requests
    if (size == 0) {
        return NULL;
    }
    
    //newsize = ALIGN(size + SIZE_T_SIZE);
    if (size <= DSIZE)
    	asize = 2 * (DSIZE);
    else
    	// asize = ALIGN(size) + DSIZE;
    	asize = (DSIZE)*((size+(DSIZE)+(DSIZE-1)) / (DSIZE));
    // Search the free list for a fit
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        #ifdef DEBUG
        mm_check(__FUNCTION__);
	#endif // DEBUG
        return bp;
    }
    
    // No fit found. Get more memory and place the block
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    #ifdef DEBUG
    mm_check(__FUNCTION__);
    #endif // DEBUG
    return bp;
}

/*
 * mm_free - Freeing a block .
 */
void mm_free(void *ptr)
{
    if (ptr == NULL)
	return;
    size_t size = GET_SIZE(HDRP(ptr));
    
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    PUT(NEXT_NODE(ptr), 0);
    PUT(PREV_NODE(ptr), 0);
    coalesce(ptr);
    #ifdef DEBUG
    mm_check(__FUNCTION__);
    #endif // DEBUG
}

/*
 * coalesce - After free,  coalesce with previous and next blocks if they
 * 	are free
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    
    if (prev_alloc && next_alloc)		// Case 1
        ;
    else if (prev_alloc && !next_alloc) {	// Case 2
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        remove_from_emptyList(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc) {	// Case 3
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        remove_from_emptyList(PREV_BLKP(bp));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else {					// Case 4
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        remove_from_emptyList(PREV_BLKP(bp));
        remove_from_emptyList(NEXT_BLKP(bp));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    insert_to_emptyList(bp);
    return bp;
}
/*
 * remove_from_emptyList - remove the block from empty list which gonna be coalesced with 
 * 	its neighbors or be malloced
 */
static void remove_from_emptyList(void *bp)
{
    char *prev = GET(PREV_NODE(bp));
    char *next = GET(NEXT_NODE(bp));
    if (prev == NULL)
    {
        if (next != NULL)
            PUT(PREV_NODE(next), 0);
        PUT(root, next);
    }
    else
    {
        if (next != NULL) 
            PUT(PREV_NODE(next), prev);
        PUT(NEXT_NODE(prev), next);
    }
    PUT(NEXT_NODE(bp), 0);
    PUT(PREV_NODE(bp), 0);
}

/*
 * insert_to_emptyList - insert free block to empty list 
 */
static void insert_to_emptyList(void *bp)
{
    char *next, *pre;
    next = (char *)(GET(root));
    if (next == NULL) {
        PUT(root, bp);
        return;
    }
    if (next > bp) {
        PUT(PREV_NODE(next), bp);
        PUT(NEXT_NODE(bp), next);
        PUT(root, bp);
        return;
    }
    while (next != NULL && bp > next) {
        pre = next;
        next = GET(NEXT_NODE(next));
    }   
    if (next != NULL)
        PUT(PREV_NODE(next), bp);
    PUT(NEXT_NODE(bp), next);
    PUT(PREV_NODE(bp), pre);
    PUT(NEXT_NODE(pre), bp);
}
 
/*
 * find_fit - search the free linked list for a feasible free block 
 *		first-fit
 */
static void *find_fit(size_t asize)
{
    // First-fit search
    char *bp;
    for (bp = GET(root); bp != NULL; bp = GET(NEXT_NODE(bp))) {
    	if (asize <= GET_SIZE(HDRP(bp))) {
    	    return bp;
    	 }
    }
    return NULL;	// No fit
}

/*
 * place -  
 */ 
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    remove_from_emptyList(bp);
	
    if ((csize - asize) >= (2 * DSIZE)) {
    	PUT(HDRP(bp), PACK(asize, 1));
	PUT(FTRP(bp), PACK(asize, 1));
	bp = NEXT_BLKP(bp);
	PUT(HDRP(bp), PACK(csize - asize, 0));
	PUT(FTRP(bp), PACK(csize - asize, 0));
	
	PUT(NEXT_NODE(bp), 0);
	PUT(PREV_NODE(bp), 0);
	coalesce(bp);
    }
    else {
	PUT(HDRP(bp), PACK(csize, 1));
	PUT(FTRP(bp), PACK(csize, 1));
    }  
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }
    if (ptr == NULL) 
        return mm_malloc(size);
    newptr = mm_malloc(size);
    if (newptr == NULL)	// if failed
      return NULL;
    copySize = GET_SIZE(HDRP(ptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr; 
}

int mm_check(char *function)
{
    printf("---cur function:%s empty blocks:\n",function);
    char *tmpP = GET(root);
    int count_empty_block = 0;
    while(tmpP != NULL)
    {
        count_empty_block++;
        printf("address：%x size:%d \n",(int)tmpP,GET_SIZE(HDRP(tmpP)));
        tmpP = GET(NEXT_NODE(tmpP));
    }
    printf("empty_block num: %d\n",count_empty_block);
    return 0;
}
