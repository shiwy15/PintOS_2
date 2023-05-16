#ifndef __LIB_KERNEL_HASH_H
#define __LIB_KERNEL_HASH_H

/* Hash table.
 *
 * This data structure is thoroughly documented in the Tour of
 * Pintos for Project 3.
 *
 * This is a standard hash table with chaining.  To locate an
 * element in the table, we compute a hash function over the
 * element's data and use that as an index into an array of
 * doubly linked lists, then linearly search the list.
 *
 * The chain lists do not use dynamic allocation.  Instead, each
 * structure that can potentially be in a hash must embed a
 * struct hash_elem member.  All of the hash functions operate on
 * these `struct hash_elem's.  The hash_entry macro allows
 * conversion from a struct hash_elem back to a structure object
 * that contains it.  This is the same technique used in the
 * linked list implementation.  Refer to lib/kernel/list.h for a
 * detailed explanation.
 
 해시 테이블은 데이터를 저장하고 검색하기 위한 자료구조입니다. 
 이 테이블은 연결 리스트를 사용하여 충돌이 발생할 때 체이닝 기법을 사용합니다. 
 요소를 테이블에서 찾기 위해서는 요소의 데이터에 대한 해시 함수를 계산하여 
 이를 이용하여 이중 연결 리스트 배열의 인덱스로 사용한 다음, 해당 리스트에서 
 선형 검색을 수행합니다.
이 테이블은 동적 할당을 사용하지 않습니다. 대신, 해시에 속할 수 있는 모든 구조체는 
struct hash_elem 멤버를 내장해야 합니다. 모든 해시 함수는 이 struct hash_elem들을
 대상으로 동작합니다. hash_entry 매크로를 사용하여 struct hash_elem을 포함하는 
 구조체 객체로의 변환을 수행할 수 있습니다. 이는 연결 리스트 구현에서 사용되는 
 기술과 동일합니다. 자세한 설명은 lib/kernel/list.h 파일을 참조하시면 됩니다.

요약하면, 이 해시 테이블은 데이터를 저장하기 위해 해시 함수와 체이닝을 사용하는 
자료구조로, 동적 할당을 사용하지 않고 해시에 속하는 구조체는 struct hash_elem을
 내장해야 합니다. 이를 통해 요소를 검색하고 저장할 수 있습니다.*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "list.h"

/* Hash element. */
struct hash_elem
{
	struct list_elem list_elem;
};

/* Converts pointer to hash element HASH_ELEM into a pointer to
 * the structure that HASH_ELEM is embedded inside.  Supply the
 * name of the outer structure STRUCT and the member name MEMBER
 * of the hash element.  See the big comment at the top of the
 * file for an example. */
#define hash_entry(HASH_ELEM, STRUCT, MEMBER) \
	((STRUCT *)((uint8_t *)&(HASH_ELEM)->list_elem - offsetof(STRUCT, MEMBER.list_elem)))

/* Computes and returns the hash value for hash element E, given
 * auxiliary data AUX. */
typedef uint64_t hash_hash_func(const struct hash_elem *e, void *aux);

/* Compares the value of two hash elements A and B, given
 * auxiliary data AUX.  Returns true if A is less than B, or
 * false if A is greater than or equal to B. */
typedef bool hash_less_func(const struct hash_elem *a,
							const struct hash_elem *b,
							void *aux);

/* Performs some operation on hash element E, given auxiliary
 * data AUX. */
typedef void hash_action_func(struct hash_elem *e, void *aux);

/* Hash table. */
struct hash
{
	size_t elem_cnt;	  /* Number of elements in table. */
	size_t bucket_cnt;	  /* Number of buckets, a power of 2. */
	struct list *buckets; /* Array of `bucket_cnt' lists. */
	hash_hash_func *hash; /* Hash function. */
	hash_less_func *less; /* Comparison function. */
	void *aux;			  /* Auxiliary data for `hash' and `less'. */
};

/* A hash table iterator. */
struct hash_iterator
{
	struct hash *hash;		/* The hash table. */
	struct list *bucket;	/* Current bucket. */
	struct hash_elem *elem; /* Current hash element in current bucket. */
};

/* Basic life cycle. */
bool hash_init(struct hash *, hash_hash_func *, hash_less_func *, void *aux);
void hash_clear(struct hash *, hash_action_func *);
void hash_destroy(struct hash *, hash_action_func *);

/* Search, insertion, deletion. */
struct hash_elem *hash_insert(struct hash *, struct hash_elem *);
struct hash_elem *hash_replace(struct hash *, struct hash_elem *);
struct hash_elem *hash_find(struct hash *, struct hash_elem *);
struct hash_elem *hash_delete(struct hash *, struct hash_elem *);

/* Iteration. */
void hash_apply(struct hash *, hash_action_func *);
void hash_first(struct hash_iterator *, struct hash *);
struct hash_elem *hash_next(struct hash_iterator *);
struct hash_elem *hash_cur(struct hash_iterator *);

/* Information. */
size_t hash_size(struct hash *);
bool hash_empty(struct hash *);

/* Sample hash functions. */
uint64_t hash_bytes(const void *, size_t);
uint64_t hash_string(const char *);
uint64_t hash_int(int);

#endif /* lib/kernel/hash.h */