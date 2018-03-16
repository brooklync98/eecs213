/*
 * The allocator is implemented using an implicit list of blocks of memory. Each block consists of a header that contains the size of the payload along with with an allocation flag, the payload, and a footer that contains the same information as the header.
 *
 *  Allocated and free block looks like:
    -------------------
 *  | block size  | a |  HEADER (4 bytes)
 *  -------------------
 *  |                 |
 *  |    payload      |
 *  |                 |
 *  -------------------
 *  |   padding      |   (if necessary)
 *  -------------------
 *  | block size  | a |  FOOTER (4 bytes)
 *  |             |   |
 *  -------------------
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
team_t team = {
    /* Team name */
    "35",
    /* First member's full name */
    "Brooklyn Copeland",
    /* First member's email address */
    "brooklyncopeland@u.northwestern.edu",
    /* Second member's full name (leave blank if none) */
    "Madeline LeFevour",
    /* Second member's email address (leave blank if none) */
    "madelinelefevour2020@u.northwestern.edu"
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE 4                 /* Word and header/footer size (bytes) */
#define DSIZE 8                 /* Double word size (bytes) */
#define CHUNKSIZE (1<<12)       /* Extend head by this amt (bytes) */
#define MIN_BLOCK_SIZE 24

#define MAX(x,y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)          (*(unsigned int*) (p))
#define PUT(p, val)     (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* Given block ptr "ptr", compute address of its header and footer */
#define HDRP(ptr)        ((char *)(ptr) - WSIZE)
#define FTRP(ptr)        ((char *)(ptr) + GET_SIZE(HDRP(ptr)) - DSIZE)

/* Given block ptr "ptr", compute address of next and previous blocks*/
#define NEXT_BLKP(ptr)       ((char *)(ptr) + GET_SIZE(((char *)(ptr) - WSIZE)))
#define PREV_BLKP(ptr)       ((char *)(ptr) - GET_SIZE(((char *)(ptr) - DSIZE)))


static char *heap_listp;

/* helper function declarations */

static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void *ptr, size_t asize);
static void *coalesce(void *ptr);
int mm_check(void);

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    
    PUT(heap_listp, 0);                             /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));    /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));    /* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));        /*Epilogue header */
    heap_listp += (2*WSIZE);
    
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

/*
 * extend_heap - extends heap to create room for new blocks
 *     Always allocate a block whose size is a multiple of the alignment.
 */

static void *extend_heap(size_t words)
{
    char *ptr;
    size_t size;
    
    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(ptr = mem_sbrk(size)) == -1)
        return NULL;
    
    /* Initialize free block header/footer and the epilogue */
    PUT(HDRP(ptr), PACK(size, 0));           /* free block header */
    PUT(FTRP(ptr), PACK(size, 0));           /* free block footer */
    PUT(HDRP(NEXT_BLKP(ptr)), PACK(0, 1));   /* new epilogue header */
    
    /* Coalesce if the previous block was free */
    return coalesce(ptr);
    
    
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *ptr;
    
    /* ignore bad spurious requests */
    if (size == 0)
        return NULL;
    
    /* Adjust block size to include overhead and alignment reqs */
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    
    /* search the free list for a fit */
    if ((ptr = find_fit(asize)) != NULL){
        place(ptr, asize);
        return ptr;
    }
    
    /* no fit found. get more mem and place on block */
    
    extendsize = MAX(asize, CHUNKSIZE);
    if((ptr = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    
    place(ptr, asize);
    return ptr;
}

/*
 * find_fit - performs first-fit search of the implicit free list
 *
 */
static void *find_fit(size_t asize)
{
    /*first-fit search */
    void *ptr;
    
    for(ptr = heap_listp; GET_SIZE(HDRP(ptr)) > 0; ptr = NEXT_BLKP(ptr)){
        if(!GET_ALLOC(HDRP(ptr)) && (asize <= GET_SIZE(HDRP(ptr)))){
            return ptr;
        }
    }
    
    return NULL; /* no fit */
    
    
}


/*
 * place -
 *
 */
static void place(void *ptr, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(ptr));
    
    if((csize - asize) >= (2*DSIZE)){
        PUT(HDRP(ptr), PACK(asize, 1));
        PUT(FTRP(ptr), PACK(asize, 1));
        ptr = NEXT_BLKP(ptr);
        PUT(HDRP(ptr), PACK(csize-asize, 0));
        PUT(FTRP(ptr), PACK(csize-asize, 0));
        
    }
    
    else{
        PUT(HDRP(ptr), PACK(csize, 1));
        PUT(FTRP(ptr), PACK(csize, 1));
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

/* coalesce - combining free blocks to avoid false fragmentation */

static void *coalesce(void *ptr)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(ptr)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    size_t size = GET_SIZE(HDRP(ptr));
    
    //case 1 --> both adjacent blocks are allocated, no coalescing possible
    if (prev_alloc && next_alloc){
        return ptr;
    }
    //case 2 --> current block is merged with the next block
    else if (prev_alloc && !next_alloc){
        size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        PUT(HDRP(ptr), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size, 0));
    }
    // case 3 --> previous block is merged with current block
    else if(!prev_alloc && next_alloc){
        size += GET_SIZE(HDRP(PREV_BLKP(ptr)));
        PUT(FTRP(ptr), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        ptr = PREV_BLKP(ptr);
        
    }
    // case 4 --> all three blocks are merged to form single block
    else{
        size += GET_SIZE(HDRP(PREV_BLKP(ptr))) + GET_SIZE(FTRP(NEXT_BLKP(ptr)));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(size, 0));
        ptr = PREV_BLKP(ptr);
    }
    
    return ptr;
}




/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    size_t copySize = GET_SIZE(HDRP(ptr));
    void *oldptr = ptr;
    void *newptr;
    size_t asize = MAX(ALIGN(size) + DSIZE, MIN_BLOCK_SIZE);
    //size_t newBlockSize = size + (2*WSIZE);
    
    //check to make sure size is not less than or equal to 0. If so, return null or (
    // if equal to 0, free the ptr and return null
    
    if(size <= 0){
        mm_free(oldptr);
        return NULL;
    }
    
    //if the sizes are the same, don't need to reallocate anything, and you can return same ptr
    
    if (asize == copySize) {
        return oldptr;
    }
    
    if(asize <= copySize){
        
        size = asize;
        /*if the leftover size of the block after the new block is placed is less than the min_block_size,
        // there is not enough space to create a new block, thus, we should just return the
            ptr without updating the headers or footers
        */
        if (copySize-size <= MIN_BLOCK_SIZE) {
            return oldptr;
        }
        PUT(HDRP(oldptr), PACK(size, 1));
        PUT(FTRP(oldptr), PACK(size, 1));
        PUT(HDRP(NEXT_BLKP(oldptr)), PACK(size, 1));
        mm_free(NEXT_BLKP(oldptr));
        return oldptr;
    }
    
    newptr = mm_malloc(size);
    
    
    if(size < copySize)
        copySize = size;
    memcpy(newptr, ptr, copySize);
    
    /* Free the old block. */
    mm_free(ptr);
    
    return newptr;
}


/*
 * mm_check - checks heap consistency when change to heap is made
 */
int mm_check(void){
    
    char *tempPtr=0;
    
    
    //check to see if allocated blocks overlap
    tempPtr = heap_listp;
    while (heap_listp) {
        if(FTRP(tempPtr) >= HDRP(NEXT_BLKP(tempPtr))){
            return 0;
        }
        tempPtr = NEXT_BLKP(tempPtr);
    }
    
    
    
    
    //return 1 if everything works
    return 1;
    
}
