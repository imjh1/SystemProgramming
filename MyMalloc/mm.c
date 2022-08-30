/*
 * mm.c - implemented by segregated free list
 * 
 * In this approach, a block is allocated by searching free block in 
 * segregated free list. There are header and footer which represents
 * size and allocation flag of blocks. When Block is freed, Block is 
 * coalesced and inserted into free list. Realloc is implemented using
 * mm_malloc and mm_free when block size cannot be adjusted. Else, 
 * realloc adjust size of allocated block.
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
 * provide your information in the following struct.
 ********************************************************/
team_t team = {
    /* Your student ID */
    "20171683",
    /* Your full name*/
    "Jonghwan Lim",
    /* Your email address */
    "limjh2255@u.sogang.ac.kr",
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

////////////////////// 교재에서 사용한 macro ///////////////////////////
/* Basic constants and macros */
#define WSIZE 		4	/* Word and header/footer size (bytes) */
#define DSIZE 		8	/* Double word size (bytes) */
#define CHUNKSIZE 	(1<<12)	/* Extend heap by this amount (bytes) */

#define MAX(x, y)	((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)		(*(unsigned int *)(p))		
#define PUT(p, val)	(*(unsigned int *)(p)) = (val)	

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)	(GET(p) & ~0x7)	
#define GET_ALLOC(p)	(GET(p) & 0x1)	

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)	((char *)(bp) - WSIZE)					
#define FTRP(bp)	((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)		

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)	((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))	
#define PREV_BLKP(bp)	((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))	
////////////////////////////////////////////////////////////////////////

/* 추가로 정의한 macro */
#define CLASS_ZERO_SIZE 2*DSIZE						// class 0의 size
#define CLASS 30							// segregated list의 class 수
#define PUT_PTR(p, ptr) (*(unsigned int *)(p)) = (unsigned int)(ptr)	// p에 ptr 주소 저장. free block의 next, prev free block주소를 저장하기 위해 사용
#define NEXT_FREE_BLKP(bp) ((char*)(bp))				// free list의 next free block주소 저장 위치
#define PREV_FREE_BLKP(bp) ((char*)(bp) + WSIZE)			// free list의 prev free block주소 저장 위치

static void *extend_heap(size_t words);		// heap의 size를 words크기만큼 증가시키는 함수
static void *coalesce(void *bp);		// bp의 prev, next block이 free일 경우 coalesce하는 함수
static void *find_fit(size_t asize);		// asize 크기의 block을 allocate할 수 있는 free block 찾는 함수
static void place(void *bp, size_t asize);	// bp에 asize 크기의 block을 allocate
static void insert_list(void *bp, size_t asize);// bp를 class에 맞는 segregated list에 삽입
static void delete_list(void *bp, int class);	// bp를 segregated list에서 제거

/* Heap Consistency Checker */
static int mm_check();
static int find_free_bp(void *bp);

void *segregated_list[CLASS];	// segregated list

/* mm_init - initialize the malloc package.*/
int mm_init(void)
{
    void* bp;
    /* Create the initial empty heap */
    if((bp = mem_sbrk(4*WSIZE)) == (void *)-1)
	return -1;

    PUT(bp, 0);				/* Alignment padding */
    PUT(bp + (1*WSIZE), PACK(DSIZE, 1));/* Prologue header */
    PUT(bp + (2*WSIZE), PACK(DSIZE, 1));/* Prologue footer */
    PUT(bp + (3*WSIZE), PACK(0, 1));	/* Epilogue header */
    bp += (2*WSIZE);

    /* initialize segregated list */
    for(int i=0; i<CLASS; i++)
        segregated_list[i] = NULL;

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if((bp = extend_heap(CHUNKSIZE/WSIZE)) == NULL)
	return -1;

    return 0;
}

/* mm_malloc - Allocate a block by searching segregated list. 
 * Always allocate a block whose size is a multiple of the alignment. */
void *mm_malloc(size_t size)
{
    char* bp;
    
    if(size == 0)
 	return NULL;

    size = ALIGN(size) + DSIZE;	// 할당하려는 block의 크기를 double word aligning하고, header와 footer를 위해 DSIZE만큼 추가

    /* size 크기를 할당할 수 있는 free block 찾지 못한 경우 */
    if((bp = find_fit(size)) == NULL){	
	size_t extendsize = MAX(size, CHUNKSIZE);
	if((bp = extend_heap(extendsize/WSIZE)) == NULL)	// heap 영역 extend
	    return NULL;
    }

    place(bp, size);	// bp에 size크기만큼 allocate
    
//    if(!mm_check())
//        return NULL;
    
    return bp;	// block 주소 return
}

/* mm_free - Freeing a block */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));		// free하려는 block의 size

    PUT(HDRP(ptr), PACK(size, 0));		// block header의 allocate flag를 unallocate로 변경
    PUT(FTRP(ptr), PACK(size, 0));		// block footer의 allocate flag를 unallocate로 변경
    coalesce(ptr);    				// prev, next block과 coalesce 

    
//    mm_check();  
}

/* mm_realloc - Implemented simply in terms of mm_malloc and mm_free */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    
    size_t old_size = GET_SIZE(HDRP(ptr));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(ptr)));

    size_t new_size;

    if(size == 0){
	mm_free(ptr);
        return NULL;
    }

    new_size = ALIGN(size) + DSIZE;

    /* Case1: new_size가 old_size보다 작으면서 split할 수 있는 경우 */ 
    if(new_size + 2*DSIZE <= old_size){
        PUT(HDRP(ptr), PACK(new_size, 1));	// block의 크기를 new_size로 변경
        PUT(FTRP(ptr), PACK(new_size, 1));	// block의 크기를 new_size로 변경
        PUT(HDRP(NEXT_BLKP(ptr)), PACK(old_size - new_size, 0));	// 남은 block은 split하여 free block
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(old_size - new_size, 0));	// 남은 block은 split하여 free block
        insert_list(NEXT_BLKP(ptr), GET_SIZE(HDRP(NEXT_BLKP(ptr))));	// free list에 삽입
    }
    /* Case2: new_size가 old_size보다 작거나 같아 split할 수 없는 경우 */
    else if(new_size <= old_size){
	/* 기존 block 그대로 사용 */
    }
    /* Case3, 4: new_size가 old_size보다 큰 경우 */
    else{
	/* Case3: next_block을 활용하여 기존 할당된 block을 size만 늘리는 경우 */
        if(!next_alloc && (next_size + old_size >= new_size + 2*DSIZE)){//((next_size + old_size) > (newsize))){
            delete_list(NEXT_BLKP(ptr), -1);	// next_block을 free list에서 제거
            PUT(HDRP(ptr), PACK(new_size, 1));	// block의 크기를 new_size로 변경
            PUT(FTRP(ptr), PACK(new_size, 1));	// block의 크기를 new_size로 변경
            PUT(HDRP(NEXT_BLKP(ptr)), PACK(old_size + next_size - new_size, 0));	// 남은 block은 split하여 free block
            PUT(FTRP(NEXT_BLKP(ptr)), PACK(old_size + next_size - new_size, 0));	// 남은 block은 split하여 free blcok
            insert_list(NEXT_BLKP(ptr), GET_SIZE(HDRP(NEXT_BLKP(ptr))));		// free list에 삽입
        }
	/* Case4: block을 다른 공간으로 옮겨 새로 할당해야 하는 경우 */
        else{
            ptr = mm_malloc(new_size);
            if (ptr == NULL)
                return NULL;
            memcpy(ptr, oldptr, old_size);
            mm_free(oldptr);
        }
    }
    
//    if(!mm_check())
//	return NULL;
    
    return ptr;
}

/* heap을 extend하여 새로운 free block을 생성하고, 해당 block의 pointer return */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if((long)(bp=mem_sbrk(size)) == -1)
	return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));		/* Free block header */
    PUT(FTRP(bp), PACK(size, 0));		/* Free block footer */
    PUT_PTR(NEXT_FREE_BLKP(bp), NULL);		// next free block 주소 NULL로 초기화
    PUT_PTR(PREV_FREE_BLKP(bp), NULL);		// prev free block 주소 NULL로 초기화
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));	/* New epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/* block을 coalescing하여 pointer return */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));	// bp의 prev block이 free block인지 check
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));	// bp의 next block이 free block인지 check
    size_t size = GET_SIZE(HDRP(bp));			// bp의 size

    /* Case 1: prev block과 next block 모두 allocated */
    if(prev_alloc && next_alloc){
	/* Nothing to do */
    }
	
    /* Case 2: prev block이 allocated, next block이 free인 경우 */
    else if(prev_alloc && !next_alloc){
	/* next block과 coalescing */
	delete_list(NEXT_BLKP(bp), -1);		// next block을 우선 segregated list에서 제거하고
	size += GET_SIZE(HDRP(NEXT_BLKP(bp)));	// bp와 next block coalesce
	PUT(HDRP(bp), PACK(size, 0));		// 변경된 block의 size 저장
	PUT(FTRP(bp), PACK(size, 0));		// 변경된 block의 size 저장
    }

    /* Case 3: prev block이 free, next block allocated된 경우 */
    else if(!prev_alloc && next_alloc){
	/* prev block과 coalescing */
	delete_list(PREV_BLKP(bp), -1);		// prev block을 우선 segregated list에서 제거하고
	size += GET_SIZE(HDRP(PREV_BLKP(bp)));	// bp와 prev block coalesce
	bp = PREV_BLKP(bp);			// coalesce된 block의 시작 주소는 coalesce하기 전 bp의 prev block의 주소
	PUT(HDRP(bp), PACK(size, 0));		// 변경된 block의 size 저장
	PUT(FTRP(bp), PACK(size, 0));		// 변경된 block의 size 저장
    }

    /* Case 4: prev block과 next block 모두 free인 경우 */
    else{
	/* prev block, next block과 coalescing */
	delete_list(PREV_BLKP(bp), -1);		// prev block segregated list에서 제거
	delete_list(NEXT_BLKP(bp), -1);		// next block segregated list에서 제거
	size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));	// bp, prev block, next block coalesce
	bp = PREV_BLKP(bp);			// coalesce한 block의 시작 주소는 coalesce하기 전 bp의 prev block의 주소
	PUT(HDRP(bp), PACK(size, 0));		// 변경된 block의 size 저장
	PUT(FTRP(bp), PACK(size, 0));		// 변경된 block의 size 저장
    }

    /* coalesce한 freed block, extended block segregated list에 삽입 */
    insert_list(bp, size);	
    return bp;
}

/* asize크기를 allocate 할 수 있는 free block을 찾아 해당 block의 pointer return */
static void *find_fit(size_t asize)
{
    void *bp;
    size_t class_size = CLASS_ZERO_SIZE;

    for(int i=0; i<CLASS; i++){
	/* allocate 하려는 block의 asize가 class보다 큰 경우 */
	if(i < CLASS-1 && asize > class_size){	
	    /* 해당 class의 free_list 탐색할 필요 X */
	}
	/* asize가 class보다 작은 경우 */
	else{
	    /* 해당 class의 free_list를 탐색 */
	    bp = segregated_list[i];
	    while(bp != NULL){
		/* asize 보다 큰 free block을 찾은 경우(first fit) */
	        if(asize <= GET_SIZE(HDRP(bp))){
		    return bp;
		}
	        bp = *(char**)NEXT_FREE_BLKP(bp);
	    }	
	}

	/* class에 asize 크기의 block을 allocate할 수 있는 block이 없는 경우
	다음 class의 free list 탐색 */
	class_size <<= 1;
    }

    /* 탐색 실패시 NULL return */
    return NULL;
}

/* bp가 가리키는 block에 asize만큼 allocate */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));			// allocate 하기 전 free blcok bp의 size

    delete_list(bp, -1);				// bp를 segregated list에서 제거
    /* allocate 하려는 size가 free block의 size보다 작은 경우 split */
    if((csize - asize) >= (2*DSIZE)){	
	PUT(HDRP(bp), PACK(asize, 1));			// bp의 header를 asize크기의 allocate block으로 설정
	PUT(FTRP(bp), PACK(asize, 1));			// bp의 footer를 asize크기의 allocate block으로 설정

	void* next_bp = NEXT_BLKP(bp);			// bp의 block을 allocate하고 남은 free space
	PUT(HDRP(next_bp), PACK(csize-asize, 0));	// (csize-asize)크기의 free block으로 설정
	PUT(FTRP(next_bp), PACK(csize-asize, 0));	// (csize-asize)크기의 free block으로 설정
	insert_list(next_bp, csize-asize);		// split된 free block을 segregated list에 삽입
    }

    /* allocate 하려는 size가 free block의 size와 같은 경우 */
    else{
	PUT(HDRP(bp), PACK(csize, 1));	// bp를 allocate block으로 설정
	PUT(FTRP(bp), PACK(csize, 1));	// bp를 allocate block으로 설정
    }
}

/* segregated list에 bp가 가리키는 asize크기의 free block 삽입 */
static void insert_list(void *bp, size_t asize){
    int insert_class = CLASS-1;
    size_t class_size = CLASS_ZERO_SIZE;

    for(int i=0; i<CLASS-1; i++){
	/* bp의 size가 class의 max_size보다 작은 경우 해당 class에 bp 삽입*/
	if(asize <= class_size){
	    insert_class = i;
	    break;
	}
	class_size <<= 1;
    }    

    /* 기존 class에 free block이 존재하는 경우 */
    if(segregated_list[insert_class] != NULL){
	PUT_PTR(PREV_FREE_BLKP(segregated_list[insert_class]), bp);	// 기존 head의 PREV FREE BLOCK은 bp를 가리키고
        PUT_PTR(NEXT_FREE_BLKP(bp), segregated_list[insert_class]);	// bp의 NEXT FREE BLOCK은 기존 head를 가리킴
    }
    /* 기존 class가 empty인 경우 */
    else
	PUT_PTR(NEXT_FREE_BLKP(bp), 0);				// bp의 NEXT FREE BLOCK은 NULL

    /* bp가 class의 header가 됨(LIFO) */
    segregated_list[insert_class] = bp;
    PUT_PTR(PREV_FREE_BLKP(bp), NULL);
}

/* free block bp를 segregated list에서 제거 */
static void delete_list(void *bp, int class){
    size_t class_size = CLASS_ZERO_SIZE;
    /* parameter로 bp의 class가 주어지지 않은 경우 bp가 저장된 class 찾기 */
    if(class == -1){
	size_t bp_size = GET_SIZE(HDRP(bp));

        for(int i=0; i<CLASS-1; i++){
	    if(bp_size <= class_size){
		class = i;
		break;
	    }
	    class_size <<= 1;
	}
	if(class == -1)
	    class = CLASS-1;
    }

    /* bp가 list의 header인 경우 */
    if(segregated_list[class] == bp){
	/* bp가 class의 유일한 block이 아닌 경우 */
	if(*(char**)NEXT_FREE_BLKP(bp) != NULL){
	    /* bp의 NEXT FREE BLOCK이 class의 새로운 header가 됨 */
	    PUT_PTR(*(char**)NEXT_FREE_BLKP(bp) + WSIZE, NULL);	
	    segregated_list[class] = *(char**)NEXT_FREE_BLKP(bp);
	}
	
	/* bp가 class의 유일한 block인 경우 */
	else{
	    segregated_list[class] = NULL;
	}
    }

    /* bp가 list의 header가 아닌 경우 */
    else{
	/* bp가 class의 마지막 block이 아닌 경우 */
	if(*(char**)NEXT_FREE_BLKP(bp) != NULL){
	    /* bp의 PREV FREE BLOCK의 next free block을 bp의 NEXT FREE BLOCK으로 변경 */
	    PUT_PTR(NEXT_FREE_BLKP(*(char**)PREV_FREE_BLKP(bp)), *(char**)NEXT_FREE_BLKP(bp));
	    /* bp의 NEXT FREE BLOCK의 prev free block을 bp의 PREV FREE BLOCK으로 변경 */
	    PUT_PTR(PREV_FREE_BLKP(*(char**)NEXT_FREE_BLKP(bp)), *(char**)PREV_FREE_BLKP(bp));
	}
	/* bp가 class의 마지막 block인 경우 */
	else{
	    /* bp의 PREV FREE BLOCK이 class의 마지막 block이 됨 */
	    PUT_PTR(NEXT_FREE_BLKP(*(char**)PREV_FREE_BLKP(bp)), NULL);
	}
    }
}

/* Heap Consistency Checker */
static int mm_check(){
    void *bp;
    /* Is every block in the free list marked as free? */
    for(int i=0; i<CLASS; i++){
	bp = segregated_list[i];	// i class list의 header
	while(bp != NULL){
	    if(GET_ALLOC(HDRP(bp))){	// allocation flag가 1인 경우
		printf("block in the free list not marked as free\n");
		return 0;		// 오류 
	    }
	    bp = *(char**)NEXT_FREE_BLKP(bp);	// check next free block
	}
    }

    bp = mem_heap_lo() + DSIZE;		// heap의 시작 block
    while(bp != mem_heap_hi() + 1){	// heap의 마지막 block전 까지 check
	/* bp가 free block인 경우 */
	if(!GET_ALLOC(HDRP(bp))){	
	    /* Are there any contiguous free blocks that somehow escaped coalescing? */
	    /* bp의 이전 block과 다음 block이 free 인지 확인 */
	    if(!GET_ALLOC(HDRP(PREV_BLKP(bp))) || !GET_ALLOC(HDRP(NEXT_BLKP(bp)))){ 
		printf("There is a free block that escaped coalescing\n");
		return 0;
	    }
	    /* Is every free block actually in the free list? */
	    /* Segregated list에 bp가 point하는 block 존재하는지 확인 */
	    if(!find_free_bp(bp)){
		printf("free block not in the free list\n");
		return 0;
	    }
	}
	bp = NEXT_BLKP(bp);	// check next block
    }

    /* Heap is Consistent */
    return 1;
}

/* bp가 가리키는 free block이 free list에 존재하는지 check */
static int find_free_bp(void* bp){
    size_t size = GET_SIZE(HDRP(bp));
    size_t class_size = CLASS_ZERO_SIZE;
    int class = CLASS-1;
    /* find class of bp */
    for(int i=0; i<CLASS-1; i++){
        if(size <= class_size){
	    class = i;
	    break;
	}
	class_size <<= 1;
    }
    /* search class */
    void* ptr = segregated_list[class];
    while(ptr != NULL){
	/* bp가 list에 존재하는 경우 */
	if((char*)ptr == (char*)bp)
	    return 1;
	ptr = (char*)(char**)NEXT_FREE_BLKP(bp);
    }
    /* bp가 list에 존재하지 않는 경우 */
    return 0;
}
