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
    "5 week 5 team",
    /* First member's full name */
    "Kang Cheolgu",
    /* First member's email address */
	"opopqwqw123@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */

#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4										// 워드 , 헤더, 푸터 사이즈 (바이트)
#define DSIZE 8										// 더블 워드 사이즈 (바이트)
#define CHUNKSIZE (1<<12)							// 이 양 만큼 힙을 확장해라

#define MAX(x,y) ((x) > (y)? (x) : (y))				// 둘중에 큰거 return 
#define PACK(size, alloc) ((size) | (alloc))		// PACK 매크로 사용하여 크기와 할당 여부를 하나의 워드로 패킹

/*
PACK 매크로는 크기(size)와 할당 여부(alloc)를 받아서 이를 하나의 워드로 패킹합니다.
size는 하위 비트에 위치하며, alloc은 상위 비트에 위치합니다.
| 비트 OR 연산자를 사용하여 두 값을 결합합니다.

size_t size = 16;   // 예시로 크기를 16으로 설정
int alloc = 1;      // 예시로 할당 여부를 1로 설정 (할당됨)

// PACK 매크로 사용하여 크기와 할당 여부를 하나의 워드로 패킹

word_t packedValue = PACK(size, alloc);

// 이제 packedValue는 size와 alloc 정보가 패킹된 하나의 워드를 나타냄

*/

// p 의 주소 word를 읽고 씀
#define GET(p)		(*(unsigned int *) (p))
#define PUT(p, val)	(*(unsigned int *)(p) = (val))

// 주소 p 로부터 크기를 읽고 필드를 할당
#define GET_SIZE(p)	(GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

// 특정 메모리 블록에 대한 포인터 bp가 주어졌을 때, 해당 블록의 헤더와 푸터의 주소를 계산하라는 의미
#define HDRP(bp)	((char *)(bp) - WSIZE)
#define FTRP(bp)	((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// 특정 메모리 블록에 대한 포인터 bp가 주어졌을 때, 다음 블록의 헤더와 푸터의 주소를 계산하라는 의미
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

static void *find_fit(size_t);
static void place(void *, size_t);
static void *coalesce(void *);
static void *extend_heap(size_t);
static char *heap_listp;
static char *last_bp;
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
	// 빈 initial heap 을 만든다.	
	if (( heap_listp = mem_sbrk(4 * WSIZE)) == (void *) - 1) return -1;

	PUT(heap_listp, 0);								// Alignment padding
	PUT(heap_listp + (1 * WSIZE), PACK(DSIZE,1)); // 프롤로그의 헤더
	PUT(heap_listp + (2 * WSIZE), PACK(DSIZE,1)); // 프롤로그의 푸터
	PUT(heap_listp + (3 * WSIZE), PACK(0,1));		// 에필로그 헤더
	heap_listp += (2 * WSIZE);

	if (extend_heap(CHUNKSIZE/WSIZE) == NULL) return -1;
	
	// next_fit 에 사용됨
	last_bp = (char *)heap_listp;
    return 0;
}

static void *extend_heap(size_t words){
	
	char *bp;
	size_t size;

	size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

	if ((long) (bp = mem_sbrk(size)) == -1) return NULL;

	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size, 0));
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

	return coalesce(bp);
}
/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */

void *mm_malloc(size_t size)
{
	//asize는 size를 8의 배수로 올린 크기
	size_t asize; 
	size_t extendsize; 
	char *bp;

	if (size == 0) return NULL;

	if (size <= DSIZE)
		asize = 2*DSIZE;
	else
		asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
	
	// 들어갈수 있는 크기의 블럭이 있다면 그곳에 할당
	if ((bp = find_fit(asize)) != NULL) {
		place(bp, asize);
		last_bp = bp;
		return bp;
	}
	// 들어갈수 있는 크기의 블럭이 없다면 확장해서 할당
	extendsize = MAX(asize,CHUNKSIZE);

	if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
		return NULL;

	place(bp, asize);

	last_bp = bp;
	return bp;
}

static void *find_fit2(size_t asize)
{
	/* First-fit search */
	void *bp;
	// bp 가 처음부터 시작하여 쭉 감
	for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
		// 할당 되어 있지 않고 사이즈도 맞음
		if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
			return bp;
		}
	 }
	return NULL; 
}

static void *find_fit(size_t asize) {
	// next fit
	void *bp = last_bp;

	// 저번에 할당했던 곳에서 시작
	for( bp = last_bp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
		if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
			last_bp = bp;
			return bp;
		}
	}
	// 끝까지 가도 없으면 처음부터 다시 시작해서 last_bp 까지 가면서 찾음 
	bp = heap_listp;
	while(bp < last_bp) {
		bp = NEXT_BLKP(bp);
		if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
			last_bp = bp;
			return bp;
		}
	}

	return NULL;
}

static void place(void *bp, size_t asize)
{
	// bp 가 위치한 빈 메모리 블럭의 크기
	size_t csize = GET_SIZE(HDRP(bp));
	// 빈메모리 크기에서 넣을 크기를 뺐을때 16 이상이면 
	if ((csize - asize) >= (2*DSIZE)) {
		// 한 메모리 크기를 2개로 나누어서 
		// 할당한 메모리
		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));
		// 할당하고 남은 메모리
		bp = NEXT_BLKP(bp);
		PUT(HDRP(bp), PACK(csize-asize, 0));
		PUT(FTRP(bp), PACK(csize-asize, 0));
	}
	else {
		// 두개로 나눌 크기가 안된다. 바로 할당
		PUT(HDRP(bp), PACK(csize, 1));
		PUT(FTRP(bp), PACK(csize, 1));
	}
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp){

	size_t size = GET_SIZE(HDRP(bp));

	PUT (HDRP(bp), PACK(size, 0));
	PUT (FTRP(bp), PACK(size, 0));

	coalesce(bp);
}

static void *coalesce(void *bp)
{
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));

	if (prev_alloc && next_alloc) { 
		last_bp = bp;
		return bp;
	}

	else if (prev_alloc && !next_alloc) {
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT(HDRP(bp), PACK(size,0));
		PUT(FTRP(bp), PACK(size,0));
	}

	else if (!prev_alloc && next_alloc) { 
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		PUT(FTRP(bp), PACK(size, 0));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
	}

	else {
		size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
	}
	last_bp = bp;
	return bp;
}

// 기존에 할당된 메모리 블록의 크기 변경
// `기존 메모리 블록의 포인터`, `새로운 크기`
void *mm_realloc(void *ptr, size_t size)
{
        /* 예외 처리 */
    if (ptr == NULL) // 포인터가 NULL인 경우 할당만 수행
        return mm_malloc(size);

    if (size <= 0) // size가 0인 경우 메모리 반환만 수행
    {
        mm_free(ptr);
        return 0;
    }

        /* 새 블록에 할당 */
    void *newptr = mm_malloc(size); // 새로 할당한 블록의 포인터
    if (newptr == NULL)
        return NULL; // 할당 실패

        /* 데이터 복사 */
    size_t copySize = GET_SIZE(HDRP(ptr)) - DSIZE; // payload만큼 복사
    if (size < copySize)                           // 기존 사이즈가 새 크기보다 더 크면
        copySize = size;                           // size로 크기 변경 (기존 메모리 블록보다 작은 크기에 할당하면, 일부 데이터만 복사)

    memcpy(newptr, ptr, copySize); // 새 블록으로 데이터 복사

        /* 기존 블록 반환 */
    mm_free(ptr);

    return newptr;
}
