/*
 * memlib.c - a module that simulates the memory system.  Needed because it 
 *            allows us to interleave calls from the student's malloc package 
 *            with the system's malloc package in libc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>

#include "memlib.h"
#include "config.h"

/* private variables */
static char *mem_start_brk;  /* points to first byte of heap */
static char *mem_brk;        /* points to last byte of heap */
static char *mem_max_addr;   /* largest legal heap address */ 

/* 
 * mem_init - initialize the memory system model
 *
 * [추가 설명]
 * allocator가 사용할 가짜 힙 전체를 한 번 확보하는 함수다.
 * mm.c는 이후 실제 시스템 malloc을 직접 쓰지 않고, 이 버퍼를
 * mem_sbrk()로 조금씩 늘려 가며 사용한다.
 *
 * [동작 순서]
 * 1. 실제 libc malloc()으로 MAX_HEAP 바이트를 확보한다.
 * 2. 시작 주소를 mem_start_brk에 저장한다.
 * 3. mem_brk도 시작점으로 맞춰서 "현재 힙은 비어 있음" 상태로 둔다.
 * 4. mem_max_addr를 계산해서 이후 힙 확장 시 범위 검사를 가능하게 한다.
 *
 * [함수 연결]
 * - mm_init()/mm_malloc()보다 먼저 실행되어야 한다.
 * - mem_sbrk(), mem_heap_lo(), mem_heap_hi(), mem_heapsize()는 모두
 *   여기서 세팅한 상태값을 바탕으로 동작한다.
 */
void mem_init(void)
{
    /* allocate the storage we will use to model the available VM */
    if ((mem_start_brk = (char *)malloc(MAX_HEAP)) == NULL) { /* 시뮬레이션 힙 전체 크기만큼 실제 메모리 확보 */
	fprintf(stderr, "mem_init_vm: malloc error\n");          /* 확보 실패 시 에러 메시지 출력 */
	exit(1);                                                 /* 힙 모델 없이는 진행 불가하므로 즉시 종료 */
    }

    mem_max_addr = mem_start_brk + MAX_HEAP;  /* 시뮬레이션 힙이 도달할 수 있는 최대 주소 계산 */
    mem_brk = mem_start_brk;                  /* 현재 brk를 시작점으로 두어 힙이 비어 있는 상태로 초기화 */
}

/* 
 * mem_deinit - free the storage used by the memory system model
 *
 * [추가 설명]
 * mem_init()이 확보해 둔 시뮬레이션 힙 버퍼를 정리하는 함수다.
 *
 * [함수 연결]
 * - mem_init()과 짝을 이루는 종료 처리 함수다.
 * - 보통 드라이버나 테스트 환경이 힙 시뮬레이터 사용을 마칠 때 호출한다.
 */
void mem_deinit(void)
{
    free(mem_start_brk);  /* mem_init에서 확보한 시뮬레이션 힙 버퍼 반납 */
}

/*
 * mem_reset_brk - reset the simulated brk pointer to make an empty heap
 *
 * [추가 설명]
 * backing storage는 유지한 채, 힙이 다시 비어 있는 것처럼 만들기
 * 위해 brk 포인터만 처음으로 되돌린다.
 *
 * [동작 순서]
 * - mem_brk를 mem_start_brk로 다시 맞춘다.
 * - 버퍼 안의 기존 바이트는 지우지 않지만, reset 이후에는 활성 힙
 *   바깥 영역으로 간주된다.
 *
 * [함수 연결]
 * - mem_init()/mem_sbrk()가 사용하는 시뮬레이터 상태와 이어진다.
 * - 새로운 MAX_HEAP 버퍼를 다시 할당하지 않고도 깨끗한 힙처럼
 *   trace를 재실행할 때 유용하다.
 */
void mem_reset_brk()
{
    mem_brk = mem_start_brk;  /* 힙 끝 포인터를 시작점으로 되돌려 다시 빈 힙처럼 만듦 */
}

/* 
 * mem_sbrk - simple model of the sbrk function. Extends the heap 
 *    by incr bytes and returns the start address of the new area. In
 *    this model, the heap cannot be shrunk.
 *
 * [추가 설명]
 * mm.c allocator가 실제로 의존하는 핵심 힙 확장 함수다.
 * 현재 구현에서는 mm_malloc()이 더 많은 공간이 필요할 때 직접 호출한다.
 *
 * [동작 순서]
 * 1. 현재 힙 끝 mem_brk를 old_brk에 저장한다.
 * 2. 요청이 음수이거나, 늘린 결과가 mem_max_addr를 넘으면 실패한다.
 * 3. 실패 시 errno를 설정하고 에러 메시지를 출력한 뒤 (void *)-1 반환.
 * 4. 성공 시 mem_brk를 incr만큼 앞으로 이동시키고, old_brk를 반환한다.
 *    이 old_brk가 새로 확보된 영역의 시작 주소가 된다.
 *
 * [함수 연결]
 * - 현재 starter allocator에서는 mm_malloc()이 직접 호출한다.
 * - 더 완전한 allocator로 가면 mm_init()이나 extend_heap() 같은
 *   helper도 이 함수를 사용하게 된다.
 * - mem_heap_hi(), mem_heapsize() 결과는 여기서 갱신된 brk를 반영한다.
 */
void *mem_sbrk(int incr) 
{
    char *old_brk = mem_brk;  /* 확장 전 힙 끝 저장: 성공 시 이 주소가 새 영역의 시작 주소가 됨 */

    if ( (incr < 0) || ((mem_brk + incr) > mem_max_addr)) { /* 음수 확장 또는 최대 힙 범위 초과는 허용 안 함 */
	errno = ENOMEM;                                        /* 표준 에러 코드로 메모리 부족 표시 */
	fprintf(stderr, "ERROR: mem_sbrk failed. Ran out of memory...\n"); /* 디버깅용 오류 메시지 출력 */
	return (void *)-1;                                    /* 실패를 호출자에게 특별한 포인터 값으로 알림 */
    }
    mem_brk += incr;              /* 힙 끝 포인터를 incr 바이트만큼 앞으로 이동 */
    return (void *)old_brk;       /* 방금 새로 얻은 영역의 시작 주소 반환 */
}

/*
 * mem_heap_lo - return address of the first heap byte
 *
 * [추가 설명]
 * 시뮬레이션 힙의 시작 주소를 외부에 알려주는 함수다.
 *
 * [함수 연결]
 * - 디버깅, heap checker, 드라이버에서 전체 힙 범위를 보고 싶을 때
 *   mem_heap_hi(), mem_heapsize()와 함께 쓰기 좋다.
 */
void *mem_heap_lo()
{
    return (void *)mem_start_brk;  /* 시뮬레이션 힙의 첫 바이트 주소 반환 */
}

/* 
 * mem_heap_hi - return address of last heap byte
 *
 * [추가 설명]
 * 현재 활성 힙에서 마지막으로 유효한 바이트 주소를 반환한다.
 *
 * [동작 순서]
 * - mem_brk는 "마지막 바이트 다음"을 가리키므로,
 *   실제 마지막 유효 바이트는 mem_brk - 1 이다.
 *
 * [함수 연결]
 * - 현재 힙 범위를 확인하거나 디버깅할 때
 *   mem_heap_lo(), mem_heapsize()와 함께 자주 묶어 쓸 수 있다.
 */
void *mem_heap_hi()
{
    return (void *)(mem_brk - 1);  /* mem_brk는 마지막 다음 바이트이므로 실제 마지막 유효 주소는 -1 */
}

/*
 * mem_heapsize() - returns the heap size in bytes
 *
 * [추가 설명]
 * 현재 활성 힙이 몇 바이트까지 확장되었는지 알려주는 함수다.
 *
 * [동작 순서]
 * - mem_start_brk부터 mem_brk까지의 거리 차이를 계산한다.
 *
 * [함수 연결]
 * - mem_sbrk() 호출이 누적된 결과를 그대로 반영한다.
 * - 힙이 얼마나 커졌는지 추적하는 드라이버나 디버깅 코드에 유용하다.
 */
size_t mem_heapsize() 
{
    return (size_t)(mem_brk - mem_start_brk);  /* 현재 힙 크기 = 현재 끝 - 시작 주소 */
}

/*
 * mem_pagesize() - returns the page size of the system
 *
 * [추가 설명]
 * 현재 실행 중인 시스템의 페이지 크기를 알려주는 보조 함수다.
 *
 * [함수 연결]
 * - allocator 정책과 직접 연결되지는 않지만,
 *   실험이나 디버깅에서 힙 증가량을 OS 페이지 단위와 비교할 때 참고할 수 있다.
 */
size_t mem_pagesize()
{
    return (size_t)getpagesize();  /* 운영체제가 사용하는 페이지 크기 반환 */
}
