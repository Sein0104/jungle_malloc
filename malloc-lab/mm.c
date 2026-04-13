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
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/*
 * mm_init - initialize the malloc package.
 *
 * [추가 설명]
 * 현재 starter allocator는 header/footer, free list 같은 별도 힙
 * 관리 구조를 만들지 않는다. 그래서 여기서는 초기화할 allocator
 * 전역 상태가 거의 없고, 단순히 성공을 반환한다.
 *
 * [함수 연결]
 * - mdriver가 trace를 실행하기 전에 먼저 호출한다.
 * - 이후 mm_malloc은 memlib 쪽 힙이 이미 준비되어 있다고 가정하고
 *   mem_sbrk()를 통해 힙을 늘린다.
 * - 나중에 implicit free list로 확장하면, 이 함수가 prologue,
 *   epilogue, 초기 free block을 세팅하는 시작점이 된다.
 */

int* heap_listp = NULL;
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) {
        return -1;
    }
    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));
    heap_listp += (2*WSIZE);

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) {
        return -1;
    }
    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 *
 * [추가 설명]
 * 현재 구현에서 실제 할당을 담당하는 핵심 함수다. free block을
 * 찾아 재사용하지 않고, 요청이 올 때마다 힙 끝을 늘려 새 공간을
 * 주는 bump-pointer allocator처럼 동작한다.
 *
 * [동작 순서]
 * 1. payload 크기에 metadata 저장 공간을 더한다.
 * 2. 전체 크기를 ALIGN()으로 정렬 단위에 맞춘다.
 * 3. mem_sbrk()로 그만큼 힙을 확장한다.
 * 4. 실패하면 NULL을 반환한다.
 * 5. 성공하면 새 공간 맨 앞에 원래 요청 크기를 저장하고,
 *    그 바로 뒤 payload 주소를 반환한다.
 *
 * [주의할 점]
 * mm_free()가 비어 있으므로 한 번 할당한 공간은 다시 재사용되지
 * 않는다. 따라서 성공한 mm_malloc() 호출은 매번 새로운 힙 공간을
 * 영구적으로 소비한다.
 *
 * [함수 연결]
 * - memlib.c의 mem_sbrk()에 직접 의존해 힙을 늘린다.
 * - 블록 맨 앞에 저장한 크기 정보는 나중에 mm_realloc()이 읽어서
 *   복사할 바이트 수를 결정한다.
 * - mm_free()가 아무 일도 하지 않기 때문에, 이 함수는 reusable
 *   block 탐색이나 coalescing을 전혀 수행하지 않는다.
 */

void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);  /* payload + 크기 metadata 공간을 합친 뒤 8바이트 정렬 */
    void *p = mem_sbrk(newsize);              /* 시뮬레이션 힙 끝을 newsize만큼 늘리고 시작 주소 받기 */
    if (p == (void *)-1)                      /* mem_sbrk 실패 시 더 이상 할당할 수 없음 */
        return NULL;                          /* malloc 실패를 호출자에게 알림 */
    else
    {
        *(size_t *)p = size;                  /* 블록 맨 앞 metadata에 사용자가 요청한 원래 크기 저장 */
        return (void *)((char *)p + SIZE_T_SIZE); /* metadata 바로 뒤 payload 시작 주소를 반환 */
    }
}

/*
 * mm_free - Freeing a block does nothing.
 *
 * [추가 설명]
 * 이 함수는 현재 placeholder다. 완전한 allocator라면 블록을 free
 * 상태로 표시하고 이후 할당에서 다시 사용할 수 있게 만들어야 한다.
 *
 * [현재 동작]
 * 전달받은 포인터를 무시하고 아무 상태 변경도 하지 않는다.
 * 따라서 mm_malloc()이 반환한 메모리는 절대 재활용되지 않는다.
 *
 * [함수 연결]
 * - mdriver가 free 요청이 들어온 trace에서 호출한다.
 * - mm_realloc()도 새 블록으로 복사한 뒤 기존 블록에 대해 호출한다.
 * - 하지만 비어 있는 함수라서, 현재 realloc은 old block 공간을
 *   회수하지 못한 채 사실상 버리게 된다.
 */
void mm_free(void *ptr)
{   
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) {
        return;
    }
        PUT(heap_listp, 0);  // PUT으로 상태변경 alloc 상태변경, size 를 0으로 만든다
        PUT(heap_listp + (1*WSIZE), PACK(0, 0));
        PUT(heap_listp + (2*WSIZE), PACK(0, 0));
        PUT(heap_listp + (3*WSIZE), PACK(0, 0));
        heap_listp += (4*WSIZE);
    (void)ptr;  /* 현재 구현에서는 free가 아무 일도 하지 않으므로 unused parameter 경고만 방지 */
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 *
 * [추가 설명]
 * 기존 블록 크기를 직접 조정하지 않고, 새 블록을 할당한 뒤 내용을
 * 복사하고 이전 블록을 "해제"하는 단순한 방식으로 구현되어 있다.
 *
 * [동작 순서]
 * 1. mm_malloc(size)로 새 블록을 확보한다.
 * 2. 현재 payload 포인터 바로 앞 metadata에서 예전 payload 크기를 읽는다.
 * 3. 이전 크기와 새 요청 크기 중 더 작은 값만큼 memcpy() 한다.
 * 4. 기존 블록에 대해 mm_free()를 호출한다.
 * 5. 새 블록 포인터를 반환한다.
 *
 * [함수 연결]
 * - 새 저장 공간은 mm_malloc()이 제공한다.
 * - old size를 읽을 수 있는 이유는 mm_malloc()이 반환 포인터 앞에
 *   size_t 하나를 metadata로 저장해 두기 때문이다.
 * - 마지막에 mm_free()를 호출하지만, 현재 allocator에서는 메모리를
 *   실제로 회수하지 않기 때문에 복사는 되더라도 공간은 줄지 않는다.
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;                               /* 기존 payload 시작 주소를 oldptr로 보관 */
    void *newptr;                                     /* 새로 할당받을 블록 주소 */
    size_t copySize;                                  /* 실제로 복사할 바이트 수 */

    newptr = mm_malloc(size);                         /* 새 요청 크기에 맞는 블록을 먼저 확보 */
    if (newptr == NULL)                              /* 새 블록 확보 실패 시 realloc도 실패 */
        return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE); /* payload 바로 앞 metadata에서 이전 요청 크기 읽기 */
    if (size < copySize)                             /* 새 블록이 더 작다면 그 크기까지만 복사해야 함 */
        copySize = size;                             /* 따라서 복사량을 새 요청 크기로 줄임 */
    memcpy(newptr, oldptr, copySize);                /* 기존 payload 내용을 새 블록으로 복사 */
    mm_free(oldptr);                                 /* 이전 블록 해제 요청: 현재 구현에서는 실제 회수는 안 됨 */
    return newptr;                                   /* 새 블록 주소를 반환 */
}