/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
/*----------------- project3 추가 ----------------*/
#include "threads/vaddr.h"
#include "threads/mmu.h"
#include "vm/anon.h"
#include "userprog/process.h"

struct list frame_table;	// 프레임 리스트 구조체 선언
struct list_elem *start; 	
struct lock frame_lock;
/*-----------------------------------------------*/

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
/* 모든 페이지들은 vm_init에서 생성됨 */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */

	/*----------------- project3 추가 ----------------*/
	list_init(&frame_table);		// frame_table 초기화
	start = list_begin(&frame_table);
	// recent_victim_elem = list_begin(&frame_table);
	// lock_init(&frame_lock);
	/*-----------------------------------------------*/
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
/* lazy loading에서 page fault가 나고, 커널이 새로운 페이지를 달라는 요청을 받으면, 아래 함수가 호출됨.
 * 이 함수는 upage가 이미 차지되었는지 확인하고, 차지되지 않았을 경우에 페이지 구조체를 할당하고, 
 * 페이지 타입에 맞는 적절한 초기화 함수를 세팅함으로써 페이지를 초기화 함. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {
	
	/* 주어진 vm_type이 uninit이 아닌 유효한 타입인지 체크 */
    ASSERT (VM_TYPE(type) != VM_UNINIT);
	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* upage가 이미 spt에서 사용 중인지 체크 */
	if (spt_find_page (spt, upage) == NULL) {	// 사용 중이 아닐 경우
		/* page 크기만큼 메모리 할당 */
		struct page *new_page = (struct page*)malloc(sizeof(struct page));
		/* 페이지 타입별 초기화 함수 세팅 */
		if (VM_TYPE(type) == VM_ANON)
			uninit_new (new_page, upage, init, type, aux, anon_initializer);
		else if (VM_TYPE(type) == VM_FILE)
			uninit_new (new_page, upage, init, type, aux, file_backed_initializer);

		new_page->writable = writable;
		
		/* TODO: Insert the page into the spt. */
		return spt_insert_page(spt, new_page);
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
/* 🔥 인자로 넘겨진 SPT에서 va와 대응되는 페이지 구조체를 찾아서 반환. 실패 시 NULL */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	/* 임시 페이지 생성 및 초기화 */
	struct page *page = (struct page*)malloc(sizeof(struct page));

	/* 생성된 임시 페이지에 가상주소 va 할당 : hash_find 함수 호출 시 가상주소를 사용해 페이지 검색 가능 */
	page->va = pg_round_down(va);
	/* hash_find 함수 호출 : 해시테이블에서 가상주소와 일치하는 hash_elem을 찾아서 반환 */
	struct hash_elem *e = hash_find(&spt->spt_hash, &page->hash_elem);
	
	free(page);
	
	/* 반환된 hash_elem으롤 실제 페이지 포인터를 찾음 */
	return e != NULL ? hash_entry(e, struct page, hash_elem) : NULL;	
}

/* Insert PAGE into spt with validation. */
/* 🔥 인자로 주어진 SPT에 페이지 구조체 삽입. 기존 SPT에서 가상주소가 존재하지 않는지 체크 */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	/* hash_insert : 주어진 해시테이블과 요소를 가지고 중복값을 체크하고 삽입함.
	 * NULL은 중복값이 없다는 의미임 */
	if (!hash_insert(&spt->spt_hash, &page->hash_elem))
        return true;
    else
        return  false;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
/* 🔥 삭제 프레임 결정 함수 : 페이지 교체 알고리즘의 일부
 * 알고리즘에 따라 어떤 프레임을 삭제할지 결정하고, 해당 프레임을 반환 */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. 
	 	우리가 구체적인 정책을 작성해야 함.*/
	struct thread *curr = thread_current();
    struct list_elem *e = start;

	for (start = e; start != list_end(&frame_table); start = list_next(start)) {
        victim = list_entry(start, struct frame, frame_elem);
        if (pml4_is_accessed(curr->pml4, victim->page->va))
            pml4_set_accessed (curr->pml4, victim->page->va, 0);
        else
            return victim;
    }

    for (start = list_begin(&frame_table); start != e; start = list_next(start)) {
        victim = list_entry(start, struct frame, frame_elem);
        if (pml4_is_accessed(curr->pml4, victim->page->va))
            pml4_set_accessed (curr->pml4, victim->page->va, 0);
        else
            return victim;
    }

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
/* 🔥 페이지 교체 함수 : 페이지 삭제 및 프레임 반환
 * vm_get_victim에서 삭제할 프레임을 결정한 후, 실제로 페이지를 하드디스크로 옮기는 작업 등을 구현 
 * 물리메모리가 부족할 때, 페이지 교체 알고리즘에 따라 희생될 페이지에 연결된 프레임을 반환 */
static struct frame *
vm_evict_frame (void) {
	struct frame *victim = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	swap_out(victim->page);

	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
/* 🔥 프레임을 할당받는 함수 : 페이지 할당 및 프레임을 가져오는 과정 구현 */
static struct frame *
vm_get_frame (void) {
	struct frame *frame = (struct frame*)malloc(sizeof(struct frame));	

	/* 물리메모리에서 페이지를 할당받은 후, 그 주소를 kva에 저장 
	 * PAL_USER는 유저 영역 페이지 할당을 지정하는 플래그 */
	frame->kva = palloc_get_page(PAL_USER);
	if (frame->kva == NULL) {			// 물리메모리의 모든 프레임이 사용 중인 상태를 의미			
		frame = vm_evict_frame();  		// 제거할 frame 정하기(eviction)
		frame->page = NULL;				// 해당 프레임에 연결된 페이지 정보를 제거
		return frame;
	}
	list_push_back(&frame_table, &frame->frame_elem);	// frame_table(리스트)에 삽입
	frame->page = NULL;		// 페이지 정보는 null (아직 매핑되지 않음)
	
	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	if(vm_alloc_page(VM_ANON | VM_MARKER_0, addr, 1))
    {
        vm_claim_page(addr);
        thread_current()->stack_bottom -= PGSIZE;   // 스택은 위에서부터 쌓기 때문에 주소값 위치를 페이지 사이즈씩 마이너스함
    }
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	
	/* TODO: Validate the fault */
	if (is_kernel_vaddr(addr))	// 커널 가상주소인 경우 false
		return false;

	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	void *rsp_stack = is_kernel_vaddr(f->rsp) ? thread_current()->rsp_stack : f->rsp;

	/* TODO: Your code goes here */
	if (not_present){
        if (!vm_claim_page(addr)) {
            if (rsp_stack - 8 <= addr && USER_STACK - 0x100000 <= addr && addr <= USER_STACK) {
                vm_stack_growth(thread_current()->stack_bottom - PGSIZE);
                return true;
            }
            return false;
        }
        else
            return true;
	}

	return false;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
/* 주어진 va에 페이지 할당하고, 해당 페이지에 프레임 할당 */
bool
vm_claim_page (void *va UNUSED) {
	/* spt에서 va에 해당하는 페이지를 찾아 할당 */
	struct page *page = spt_find_page(&thread_current()->spt, va);
	if (page == NULL)
		return false; 

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
/* 인자로 주어진 페이지에 물리메모리 프레임을 할당 */
static bool
vm_do_claim_page (struct page *page) {
	/* vm_get_frame 호출 : 프레임 하나 할당 */
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	if (install_page(page->va, frame->kva, page->writable))
		return swap_in(page, frame->kva);

	return false;
}

/* Initialize new supplemental page table */
/* 🔥 SPT 초기화 함수
 * 새로운 프로세스가 initd로 시작하거나, do_fork로 생성될 때 호출됨 */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->spt_hash, hash_func, less_func, NULL);
}

/* 해시테이블 초기화할 때, 해시값을 구해주는 해시함수의 포인터 
 * 해시함수는 요소를 입력받아 해당 요소의 해시값을 계산하고 반환 */
uint64_t hash_func (const struct hash_elem *e, void *aux) {
	const struct page *pg = hash_entry(e, struct page, hash_elem);
	return hash_bytes(&pg->va, sizeof(pg->va));
}

/* 해시테이블을 초기화할 때, 해시 요소들을 비교하는 함수 포인터 
 * 해시테이블에서 요소를 정렬하고 비교하기 위해 사용.
 * 가상주소를 기준으로 정렬하여 접근시간을 최소화 */
bool less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux) {
	const struct page *page_a = hash_entry(a, struct page, hash_elem);
	const struct page *page_b = hash_entry(b, struct page, hash_elem);

	return page_a->va < page_b->va;
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	struct hash_iterator i;
    hash_first (&i, &src->spt_hash);
    while (hash_next (&i)) {	// src의 각각의 페이지를 반복문을 통해 복사
        struct page *parent_page = hash_entry (hash_cur (&i), struct page, hash_elem);   // 현재 해시 테이블의 element 리턴
        enum vm_type type = page_get_type(parent_page);		// 부모 페이지의 type
        void *upage = parent_page->va;						// 부모 페이지의 가상 주소
        bool writable = parent_page->writable;				// 부모 페이지의 쓰기 가능 여부
        vm_initializer *init = parent_page->uninit.init;	// 부모의 초기화되지 않은 페이지들 할당 위해 
        void* aux = parent_page->uninit.aux;

        if (parent_page->uninit.type & VM_MARKER_0) {
            setup_stack(&thread_current()->tf);
        }
        else if(parent_page->operations->type == VM_UNINIT) {	// 부모 타입이 uninit인 경우
            if(!vm_alloc_page_with_initializer(type, upage, writable, init, aux))
                return false;
        }
        else {
            if(!vm_alloc_page(type, upage, writable))
                return false;
            if(!vm_claim_page(upage))
                return false;
        }

        if (parent_page->operations->type != VM_UNINIT) {   //! UNIT이 아닌 모든 페이지(stack 포함)는 부모의 것을 memcpy
            struct page* child_page = spt_find_page(dst, upage);
            memcpy(child_page->frame->kva, parent_page->frame->kva, PGSIZE);
        }
    }
    return true;
}

void spt_destructor(struct hash_elem *e, void* aux) {
    const struct page *p = hash_entry(e, struct page, hash_elem);
    free(p);
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	/* 해시테이블을 해제하고, 해시 테이블 내의 각 항목에 대해 
	 * spt_destructor 함수로 추가 정리 작업 진행 
	 * (페이지 테이블의 모든 엔트리를 삭제하고 메모리 해제)*/
	struct hash_iterator i;

	hash_first (&i, &spt->spt_hash);
	while (hash_next (&i)) {
		struct page *page = hash_entry (hash_cur (&i), struct page, hash_elem);

		if (page->operations->type == VM_FILE) 
			do_munmap(page->va);
	}
	hash_destroy(&spt->spt_hash, spt_destructor);
}

