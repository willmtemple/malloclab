/*
 * wmtemple-mm.c -- Implementation of malloc(), realloc(), and free()
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/* Team information. */
team_t team = {
    /* Team name */
    "bjmcmorran-wmtemple",
    /* First member's full name */
    "William Temple",
    /* First member's email address */
    "wmtemple@wpi.edu",
    /* Second member's full name (leave blank if none) */
    "Benjamin McMorran",
    /* Second member's email address (leave blank if none) */
    "bjmcmorran@wpi.edu"
};

/*
 * Function Prototypes
 */

// Extend heap by units WORDS
static void * extend_heap( size_t units );

// Coalesce adjacent free blocks
static void * coalesce( void * bp );

//Find a free block of appropriate size
static void * findSpace( size_t size );

//Allocate space at bp of size bytes
static void place( void * bp, size_t size );

void prnHeap();

/*
 * Constant definitions
 */

#define DEBUG 1

#define WORD_SIZE   4
#define DWORD_SIZE  8

#define ALIGNMENT   8
#define PAGE_SIZE   4096

#define MIN_BLK_SZ  2*DWORD_SIZE //enough for the header info and a DWORD

/* 
 * Some useful macros, based on "Computer Systems"
 *   Bryant & O'Hallaron SS 9.9.12
 */

#define MAX(x, y)           ((x)>(y)?(x):(y))

//Pack a size and alloc bit into a uint tag
#define MK_INFO(sz, al)   ((sz)|(al))

// Retreive info from journaling tags
#define GET_SIZE(tp)      (*((uint32_t *)(tp)) & ~0x7)
#define GET_ALLOC(tp)     (*((uint32_t *)(tp)) & 0x1)

// Compute header and footer pointers from block pointer
#define HDRP(bp)            (((void *)(bp)) - WORD_SIZE)
#define FTRP(bp)            (((void *)(bp)) + GET_SIZE(HDRP(bp)) - DWORD_SIZE)

//Get/Set the info tag header
#define GET_TAG(tp)         (*(uint32_t *)(tp))
#define SET_TAG(tp, tag)    (*(uint32_t *)(tp) = ((uint32_t)(tag)))

//Calculate pointer to next and prev blocks, c.o. Bryant and O'Hallaron
#define NEXT_BLKP(bp) ((void *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_FTRP(bp) ((void *)(bp) - DWORD_SIZE) //fast calc prev footer
#define PREV_BLKP(bp) ((void *)(bp) - GET_SIZE(PREV_FTRP(bp)))

//Pull the next and prev pointers off of the explicit free list
#define NEXT_FREEP(bp)  (*((void *)(bp)) + WORD_SIZE)
#define PREV_FREEP(bp)  (*((void *)(bp)))

//Set the next and prev fps from the explicit free list
#define SET_NFREEP(bp, nPtr)  ((*((void *)(bp)) + WORD_SIZE) = ((void *)(nPtr)))
#define SET_PFREEP(bp, pPtr)  ((*((void *)(bp))) = ((void *)(pPtr)))

//Compute best multiple of DWORD_SIZE to fit a given size of variable
#define DMULT(x)    (DWORD_SIZE * (((size) + DWORD_SIZE + (DWORD_SIZE-1)) / DWORD_SIZE))

//Global pointer to the start of our heap, i.e. the first free block.
static void * g_heapPtr;

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    //Start a free list with some dummy data
    if((g_heapPtr = mem_sbrk(4*WORD_SIZE)) == (void *)-1)
        return -1; //Fail if we can't get 16 bytes to start (god help us)

    uint32_t dummyTag = MK_INFO(DWORD_SIZE, 1);

    SET_TAG(g_heapPtr, 0); // This is padding.     The first two WORDS in this list
    SET_TAG(g_heapPtr + WORD_SIZE, dummyTag);  //  are a dummy entry (alloc'd) to
    SET_TAG(g_heapPtr + DWORD_SIZE, dummyTag); //  safeguard from initialization

    // This will be overwritten in a minute. We need it to avoid segfaulting.
    SET_TAG(g_heapPtr + 3*WORD_SIZE, MK_INFO(0, 1));

    g_heapPtr += DWORD_SIZE; // Set the heap list pointer between the header and footer.

    //Extend the heap by one page.
    if ( extend_heap(PAGE_SIZE/WORD_SIZE) == NULL) return -1;

    //Sehr guht.
    return 0;
}

static void * extend_heap( size_t units )
{

    void * bp;
    size_t sz = units * WORD_SIZE;

    if ((long)(bp = mem_sbrk(sz)) == -1) return NULL; //we are oom

    //Overwrite the old epilogue header with new info for this free block
    SET_TAG(HDRP(bp), MK_INFO(sz, 0)); //New header
    SET_TAG(FTRP(bp), MK_INFO(sz, 0)); //New footer

    SET_TAG(HDRP(NEXT_BLKP(bp)), MK_INFO(0, 1)); //New epilogue

    return coalesce( bp );

}

/* 
 * mm_malloc - 
 */
void * mm_malloc(size_t size)
{

    void * bp;

    if( size == 0 ) return NULL;

    //Voodoo to figure out how much memory we need for overhead and to preserve alignment.
    size_t adj_size = (size <= DWORD_SIZE)?(2*DWORD_SIZE):DMULT(size);

    if ((bp = findSpace(adj_size)) != NULL) {

        place( bp, adj_size );
        return bp;

    }

    if((bp = extend_heap(MAX(adj_size, PAGE_SIZE)/WORD_SIZE)) == NULL)
        return NULL;
 
    place(bp, adj_size);

    return bp;

}

static void * findSpace( size_t size )
{

    void * bp = g_heapPtr;
    uint32_t sz =  GET_SIZE(HDRP(bp));

    while( sz != 0 ) {

        if( (sz >= size) && !GET_ALLOC(HDRP(bp)) ) return bp;

        bp = NEXT_BLKP(bp);
        sz = GET_SIZE(HDRP(bp));

    }

    return NULL;

}

static void place( void * bp, size_t size )
{

    size_t wholesz = GET_SIZE(HDRP(bp));
    size_t remsz = wholesz - size;

    size = ( remsz < MIN_BLK_SZ )?wholesz:size;

    SET_TAG(HDRP(bp), MK_INFO(size, 1));
    SET_TAG(FTRP(bp), MK_INFO(size, 1));

    if( remsz >= MIN_BLK_SZ ) {

        SET_TAG(HDRP(NEXT_BLKP(bp)), MK_INFO(remsz, 0));
        SET_TAG(FTRP(NEXT_BLKP(bp)), MK_INFO(remsz, 0)); 

    }

}

/*
 * mm_free - Return a block to the free list.
 */
void mm_free(void *ptr)
{

    size_t sz = GET_SIZE(HDRP(ptr));

    //Set the alloc bit to zero on the header and footer
    SET_TAG(HDRP(ptr), MK_INFO(sz, 0));
    SET_TAG(FTRP(ptr), MK_INFO(sz, 0));

    // coalesce adjacent free blocks together
    coalesce(ptr);

}

static void * coalesce( void * bp )
{

    uint8_t pAlloc = GET_ALLOC(PREV_FTRP(bp));
    uint8_t nAlloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t sz = GET_SIZE(HDRP(bp));

    if( pAlloc && nAlloc ) return bp;

    else if ( !pAlloc && nAlloc ) { // Prev is free
        
        sz += GET_SIZE(PREV_FTRP(bp));
        SET_TAG(FTRP(bp), MK_INFO(sz, 0));
        SET_TAG(HDRP(PREV_BLKP(bp)), MK_INFO(sz, 0));
        bp = PREV_BLKP(bp); // set bp back to appropriate start

    } else if ( pAlloc && !nAlloc ) { // Next is free
        
        sz += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        SET_TAG(HDRP(bp), MK_INFO(sz, 0));
        SET_TAG(FTRP(bp), MK_INFO(sz, 0));
    
    } else { // both are free

        sz += GET_SIZE(PREV_FTRP(bp)) +
              GET_SIZE(HDRP(NEXT_BLKP(bp)));

        SET_TAG(HDRP(PREV_BLKP(bp)), MK_INFO(sz, 0));
        SET_TAG(FTRP(NEXT_BLKP(bp)), MK_INFO(sz, 0));

        bp = PREV_BLKP(bp);

    }

    return bp;

}

/*
 * mm_realloc
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *newptr;
    size_t copySize;

    //acts like free if size is null, acts like malloc if ptr is null
    if( size == 0 ) {

        mm_free(ptr);
        return NULL;

    } else if( ptr == NULL ) return mm_malloc(size);

    //Voodoo to figure out how much memory we need for overhead and to preserve alignment.
    size_t adj_size = (size <= DWORD_SIZE)?(2*DWORD_SIZE):DMULT(size);

    size_t cur_size = GET_SIZE(HDRP(ptr));

    if( adj_size == cur_size ) { //Pointer is exactly the right size.

        return ptr;

    } else if( adj_size < GET_SIZE(HDRP(ptr)) ) {  //Free block might be split,
                                                    //so free and place at same loc

        //Free is inlined here so that I can use the poitner returned by coalesce()
        size_t sz = GET_SIZE(HDRP(ptr));

        //Set the alloc bit to zero on the header and footer
        SET_TAG(HDRP(ptr), MK_INFO(sz, 0));
        SET_TAG(FTRP(ptr), MK_INFO(sz, 0));

        newptr = coalesce(ptr);

        if(newptr != ptr) memcpy(newptr, ptr, size);    //copy to beginning of free
                                                        //  block if ptr changes

        place( newptr, adj_size ); //split if necessary
        return newptr;

    } else { //We need more space.

        if( (newptr = mm_malloc( size )) == NULL) return NULL; //oom

        //If the new size is less than the buffer between the headers, only copy that,
        //  otherwise copy the whole existing buffer.
        copySize = ( size < (cur_size - DWORD_SIZE) )?size:(cur_size - DWORD_SIZE);

        memcpy(newptr, ptr, copySize);
        mm_free(ptr);
        return newptr;

    }

}

/*
 * mm_check checks the heap for consistency
 * This checker is relatively simple because our malloc algorithm does not
 * use an explicit free list. It checks for contigious free blocks that
 * escaped coalescing. Free list blocks really being free and free blocks
 * being on the free list are impossible to test without an explicit free
 * list. We do not use pointers in our headers, and it is impossible to
 * check if blocks overlap in our implementation because we cannot tell
 * the difference between header and payload data.
 */
int mm_check() {
    void * bp = g_heapPtr;
    size_t size = GET_SIZE(HDRP(bp));
    int prevAlloc = 1;
    int isValid = 1;

    do {
        int alloc = GET_ALLOC(HDRP(bp));
        if (!prevAlloc && !alloc) {
            printf("Contigious free block at %p.\n", bp);
            isValid = 0;
        }
        bp = NEXT_BLKP(bp);
        size = GET_SIZE(HDRP(bp));
        prevAlloc = alloc;
    } while (size != 0);

    if (isValid) {
        printf("Heap consistent.\n");
    }
    return isValid;
}

void prnHeap()
{

    void * bp = g_heapPtr;
    size_t sz = GET_SIZE(HDRP(bp));

    do {

        printf("%p:%09d, %d\n", bp, sz, GET_ALLOC(HDRP(bp)));
        bp = NEXT_BLKP(bp);
        sz = GET_SIZE(HDRP(bp));

    } while ( sz != 0 );

    printf("Reached sentinel! %p:%d\n", bp, sz);

}
