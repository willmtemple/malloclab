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
team_t team = {
    /* Team name */
    "MLG Computer Science Pro",
    /* First member's full name */
    "William Temple",
    /* First member's email address */
    "wmtemple@wpi.edu",
    /* Second member's full name (leave blank if none) */
    "Benjamin McMorran",
    /* Second member's email address (leave blank if none) */
    "bjmcmorran@wpi.edu"
};

/* K&R sec. 8.7 */

typedef struct header {
    struct header *nP;
    //struct header *pP;
    size_t sz;
} Header;

#define PAGE_SIZE 4 //16 byte page. //top kekek

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

//Size of our header ceiled to 8B
#define ALIGN_HEADER (ALIGN(sizeof(Header)))

/* Global Variables */

static Header root; // This is the root of our LinkedList
static Header * freep;

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    memset( &root, sizeof( Header ), 0 );
    root.nP = &root;
    //root.pP = &root;
    root.sz = 0;
    freep = &root;
    return 0;
}

/* morecore:  ask system for more memory */
static Header *moreMemory(unsigned n)
{

    void * np = NULL;

    // allocate either PAGE_SIZE or the smallest multiple of PAGE_SIZE
    //   not less than n
    n = ( n < PAGE_SIZE )?PAGE_SIZE:((n/PAGE_SIZE)*PAGE_SIZE);

    //Align n to smallest multiple of ALIGN_HEADER not less than n
    n = (n/ALIGN_HEADER) * ALIGN_HEADER;


    if( (np = mem_sbrk( n )) == (void *)-1 ) return NULL;

    Header * hp = (Header *)np;
    hp->sz = n - ALIGN_HEADER; 
    mm_free( (void *)hp + ALIGN_HEADER );
    return freep;

}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void * mm_malloc(size_t size)
{

    Header * prevp = freep;
    Header * p = NULL;
    size_t alsize = ALIGN( size );

    for( p = prevp->nP; ; prevp = p, p = p->nP ) {

        if( p->sz == alsize ) {

            prevp->nP = p->nP;
            freep = prevp;
            return ((void *) p) + ALIGN_HEADER;

        } else if( p->sz > (alsize + ALIGN_HEADER) ) {

            size_t oldsize = p->sz;
            p->sz = alsize;
            prevp->nP = ((void *)p) + alsize + ALIGN_HEADER;
            prevp->nP->sz = oldsize - p->sz -ALIGN_HEADER;

            freep = prevp; //Why??
            return ((void *) p) + ALIGN_HEADER;

        } else if( p == freep ) {

            if( (p = moreMemory( alsize + ALIGN_HEADER )) == NULL ) return NULL;

        }

    }
 
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{

    Header * bp = (Header *)(ptr - ALIGN_HEADER);
    Header * p;

    for( p = freep; !( bp > p && bp < p->nP); p = p->nP )
        if( p >= p->nP && ( bp > p || bp < p->nP) )
            break;

    if( ptr + bp->sz == p->nP ) { //Adjacent bottom of free block
        bp->sz += p->nP->sz + ALIGN_HEADER;
        bp->nP = p->nP->nP;
    } else bp->nP = p->nP;

    if( p + p->sz + ALIGN_HEADER == bp) {
        p->sz += bp->sz + ALIGN_HEADER;
        p->nP = bp->nP;
    } else p->nP = bp;

    freep = p;

}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}
