/*
 * Copyright (C) 2011, 2012, 2013, 2014 Mail.RU
 * Copyright (C) 2011, 2012, 2013, 2014 Yuriy Vostrikov
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* The MIT License

   Copyright (c) 2008, by Attractive Chaos <attractivechaos@aol.co.uk>

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/


/* Examples:

#define mh_name _x
#define mh_key_t int
#define mh_val_t char
#define MH_STATIC
#include "mhash.h"

int main() {
	int ret, is_missing;
	uint32_t k;
	struct mh_x_t *h = mh_x_init(NULL);
	k = mh_x_iput(h, 5, &ret);
	if (!ret)
		mh_x_del(h, k);
	*mh_x_pvalue(h, k) = 10;
	k = mh_x_get(h, 10);
	is_missing = (k == mh_end(h));
	k = mh_x_get(h, 5);
	mh_x_del(h, k);

	for (k = mh_begin(h); k != mh_end(h); ++k)
		if (mh_x_slot_occupied(h, k))
			*mh_x_pvalue(h, k) = 1;

	mh_x_destroy(h);
	return 0;
}


// map: int -> int
#define mh_name _intmap
#define mh_key_t int
#define mh_val_t int
#include "mhash.h"

// map2: int -> int, exisiting slot struct
#define mh_name _intmap2
struct intmap2_slot {
	int val;
	int val2;
	int key;
};
#define mh_slot_t intmap2_slot
#define mh_slot_key(h, slot) (slot)->key
#define mh_slot_val(slot) (slot)->val
#include "mhash.h"

// set: int
#define mh_name _intset
#define mh_key_t int
#include "mhash.h"


// string set with inline bitmap: low 2 bits of pointer used for hash housekeeping
#define mh_name _str
#define mh_slot_t cstr_slot
struct cstr_slot {
	union {
		const char *key;
		uintptr_t bits;
	};
	//int value;
};
#define mh_node_size sizeof(struct cstr_slot)

#define mh_hash(h, key) ({ MurmurHash2((key), strlen((key)), 13); })
#define mh_eq(h, a, b) ({ strcmp((a), (b)) == 0; })

# define mh_exist(h, i)		(h->slots[i].bits & 1)
# define mh_setfree(h, i)	h->slots[i].bits &= ~1
# define mh_setexist(h, i)	h->slots[i].bits |= 1
# define mh_dirty(h, i)		(h->slots[i].bits & 2)
# define mh_setdirty(h, i)	h->slots[i].bits |= 2

#define mh_slot_key(h, slot) ((const char *)((slot)->bits >> 2))
#define mh_slot_set_key(slot, key) (slot)->bits = ((slot)->bits & 3UL) | ((uintptr_t)key << 2)
#define mh_slot_copy(h, new, old) (new)->bits = (((old)->bits & ~3UL) | 1UL)
#include "m2hash.h"
*/

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#ifndef MH_HELPER_MACRO
#define MH_HELPER_MACRO
#define mh_cat(a, b) mh##a##_##b
#define mh_ecat(a, b) mh_cat(a, b)
#define _mh(x) mh_ecat(mh_name, x)
#define mh_unlikely(x)  __builtin_expect((x),0)
#endif

#ifndef MH_INCREMENTAL_RESIZE
#define MH_INCREMENTAL_RESIZE 0
/* incremental resize interacts badly with slot mutation and should be enabled explicitly */
#endif

#ifdef MH_INCREMENTAL_CONST
# undef MH_INCREMENTAL_CONST
#endif
#if MH_INCREMENTAL_RESIZE
# define MH_INCREMENTAL_CONST const
#else
# define MH_INCREMENTAL_CONST
#endif

#ifdef MH_STATIC
#define MH_SOURCE
#define MH_DECL static inline
#else
#define MH_DECL
#endif

#ifndef mh_slot_t
# ifndef mh_key_t
#  error Either mh_slot_t or mh_key_t must be defined
# endif
#define mh_slot_t struct _mh(slot)
struct _mh(slot) {
#ifdef mh_val_t
	mh_val_t val;
#define mh_slot_val(slot) (slot)->val
#endif
	mh_key_t key;
#ifndef mh_slot_key
# define mh_slot_key(h, slot) 	(slot)->key
#endif
} __attribute__((packed));
#else
# ifdef mh_key_t
#  error Either mh_slot_t or mh_key_t must be defined
# endif
# ifndef mh_slot_key
#  error mh_slot_key must be defined if mh_slot_t defined
# endif
typedef typeof(mh_slot_key(NULL, (mh_slot_t *)0)) _mh(key_t);
# define mh_key_t _mh(key_t)
# ifdef mh_slot_val
typedef typeof(mh_slot_val((mh_slot_t *)0)) _mh(val_t);
#  define mh_val_t _mh(val_t)
# endif
#endif

#ifndef mh_slot_size
#  define mh_slot_size(h) sizeof(mh_slot_t)
#endif

/* value of mh_hash(h, a) should be unsigned */
#ifndef mh_hash
# define mh_hash(h, a) mh_def_hash(a)
#endif

#ifndef mh_eq
# define mh_eq(h, a, b) mh_def_eq(a, b)
#endif

#ifndef mh_slot
# define mh_slot(h, i)		((h)->slots + i)
#endif

#ifndef mh_slot_key_eq
# define mh_slot_key_eq(h, i, key) mh_eq(h, mh_slot_key(h, mh_slot(h, i)), key)
#endif

#ifndef mh_slot_set_key
# define mh_slot_set_key(h, slot, key) memcpy(&mh_slot_key(h, slot), &(key), sizeof(mh_key_t))
#endif

#ifndef mh_slot_copy
# define mh_slot_copy(h, a, b) memcpy(a, b, mh_slot_size(h));
#endif

#ifndef mh_byte_map
# define mh_byte_map 0
#endif
#ifndef mh_custom_map
#if mh_byte_map
# if mh_byte_map == 1
#  define mh_map_t		uint8_t
#  define mh_divider		125
# elif mh_byte_map == 2
#  define mh_map_t		uint16_t
#  define mh_divider		32713
# else
#  error "mh_byte_map could be 0, 1 or 2"
# endif
# define mh_get_hashik(k)	((k % mh_divider + 1) * 2)
# define mh_exist(h, i)		(h->map[i] & (~(mh_map_t)1))
# define mh_setfree(h, i)	h->map[i] &= 1
# define mh_mayequal(h, i, hk)	(mh_exist(h, i) == (hk))
# define mh_setexist(h, i, hk)	h->map[i] |= hk
# define mh_dirty(h, i)		(h->map[i] & 1)
# define mh_setdirty(h, i)	h->map[i] |= 1
#else
# define mh_divider		1
# define mh_map_t		uint32_t
# define mh_get_hashik(k)	(1)
# define mh_exist(h, i)		(h->map[(i) >> 4] & (1u << ((i) & 0xf)))
# define mh_mayequal(h, i, hk)	({ (void)(hk); mh_exist(h, i); })
# define mh_setfree(h, i)	h->map[(i) >> 4] &= ~(1u << ((i) & 0xf))
# define mh_setexist(h, i, hk)	({ (void)(hk); h->map[(i) >> 4] |= (1u << ((i) & 0xf)); })
# define mh_dirty(h, i)		(h->map[(i) >> 4] & (1u << (((i) & 0xf) + 0x10)))
# define mh_setdirty(h, i)	h->map[(i) >> 4] |= (0x10000u << ((i) & 0xf))
#endif
#endif

#define mhash_t _mh(t)
struct _mh(t) {
	mh_slot_t *slots;
#ifndef mh_custom_map
	mh_map_t  *map;
#endif
	uint32_t node_size;
	uint32_t n_mask, n_occupied, size, upper_bound;

	uint32_t resize_position, resize_batch;
	struct mhash_t *shadow;
	void *(*realloc)(void *, size_t);
#ifdef mh_arg_t
	mh_arg_t arg;
#endif
};

/* public api */
MH_DECL struct mhash_t * _mh(init)(void *(*custom_realloc)(void *, size_t));
MH_DECL void _mh(initialize)(struct mhash_t *h);
MH_DECL void _mh(destroy)(struct mhash_t *h);
MH_DECL void _mh(destruct)(struct mhash_t *h); /* doesn't free hash itself */
MH_DECL void _mh(clear)(struct mhash_t *h);
MH_DECL size_t _mh(bytes)(struct mhash_t *h);
#define mh_size(h)		({ (h)->size; 		})
#define mh_begin(h)		({ 0;	})
#define mh_end(h)		({ (h)->n_mask + 1;	})
#define mh_foreach(name, h, x)	for (uint32_t x = 0; x <= (h)->n_mask; x++) if (mh_ecat(name, slot_occupied)(h, x))

/* basic */
static inline uint32_t _mh(get)(const struct mhash_t *h, mh_key_t const key);
/* it's safe (and fast) to set value via pvalue() pointer right after iput():
   uint32_t x = mh_name_iput(h, new_key, NULL);
   *mh_pvalue(h, x) = new_value;
 */
static inline uint32_t _mh(iput)(struct mhash_t *h, mh_key_t const key, int *ret);
static inline void _mh(del)(struct mhash_t *h, uint32_t x);
static inline int _mh(exist)(struct mhash_t *h, mh_key_t key);

/*  slot */
static inline uint32_t _mh(sget)(const struct mhash_t *h, mh_slot_t const *slot);
static inline uint32_t _mh(sget_by_key)(const struct mhash_t *h, mh_key_t key);
static inline int _mh(sremove)(struct mhash_t *h, mh_slot_t const *slot, mh_slot_t *prev_slot);
static inline int _mh(sremove_by_key)(struct mhash_t *h, mh_key_t key, mh_slot_t *prev_slot);
static inline int _mh(sput)(struct mhash_t *h, mh_slot_t const *slot, mh_slot_t *prev_slot);

/* kv */
static inline mh_key_t _mh(key)(struct mhash_t *h, uint32_t x);
#ifdef mh_val_t
static inline int _mh(put)(struct mhash_t *h, mh_key_t key, mh_val_t val, mh_val_t *prev_val);
/* as long as incremental resize is disabled and value is simple scalar (no mh_slot_set_val macro),
   it can be mutated via pvalue() pointer:

   uint32_t x = mh_name_get(h, key);
   if (x != mh_end(h))
	*mh_name_pvalue(h, x) = new_value;

   otherwise set_value() must be used:
   uint32_t x = mh_name_get(h, key);
   if (x != mh_end(h))
	mh_name_set_value(h, x, new_value);
*/
static inline void _mh(set_value)(struct mhash_t *h, uint32_t x, mh_val_t val);
static inline mh_val_t _mh(value)(struct mhash_t *h, uint32_t x);
static inline mh_val_t MH_INCREMENTAL_CONST * _mh(pvalue)(struct mhash_t *h, uint32_t x);
static inline int _mh(remove)(struct mhash_t *h, mh_key_t key, mh_val_t *prev_val);
#endif

/* internal api */
static inline mh_slot_t * _mh(slot)(const struct mhash_t *h, uint32_t x) { return mh_slot(h, x); }
MH_DECL void _mh(slot_move)(struct mhash_t *h, uint32_t dx, uint32_t sx);
MH_DECL void _mh(resize_step)(struct mhash_t *h);
MH_DECL void _mh(start_resize)(struct mhash_t *h, uint32_t buckets);

static inline void _mh(slot_copy)(struct mhash_t *d, uint32_t dx, mh_slot_t const *source);
MH_DECL void _mh(slot_copy_to_shadow)(struct mhash_t *h, uint32_t o, int exist);

#define mh_malloc(h, size) (h)->realloc(NULL, (size))
#define mh_calloc(h, nmemb, size) ({			\
	size_t __size = (size) * (nmemb);		\
	void *__ptr = (h)->realloc(NULL, __size);	\
	memset(__ptr, 0, __size);			\
	__ptr; })
#define mh_free(h, ptr) (h)->realloc((ptr), 0)

#ifdef MH_DEBUG
MH_DECL void _mh(dump)(struct mhash_t *h);
#endif


#ifndef mh_def_hash
static inline unsigned mh_u64_hash(uint64_t kk) {
	/* from super fast hash :-) */
	kk ^= kk >> 23;
	kk *= 0x2127599bf4325c37ULL;
	return kk ^ (kk >> 47);
}
//-----------------------------------------------------------------------------
// MurmurHash2, by Austin Appleby

// Note - This code makes a few assumptions about how your machine behaves -

// 1. We can read a 4-byte value from any address without crashing
// 2. sizeof(int) == 4

// And it has a few limitations -

// 1. It will not work incrementally.
// 2. It will not produce the same results on little-endian and big-endian
//    machines.

static inline unsigned int mh_MurmurHash2 ( const void * key, int len, unsigned int seed )
{
	// 'm' and 'r' are mixing constants generated offline.
	// They're not really 'magic', they just happen to work well.

	const unsigned int m = 0x5bd1e995;
	const int r = 24;

	// Initialize the hash to a 'random' value

	unsigned int h = seed ^ len;

	// Mix 4 bytes at a time into the hash

	const unsigned char * data = (const unsigned char *)key;

	while(len >= 4)
	{
		unsigned int k = *(unsigned int *)data;

		k *= m;
		k ^= k >> r;
		k *= m;

		h *= m;
		h ^= k;

		data += 4;
		len -= 4;
	}

	// Handle the last few bytes of the input array

	switch(len)
	{
	case 3: h ^= data[2] << 16;
	case 2: h ^= data[1] << 8;
	case 1: h ^= data[0];
	        h *= m;
	};

	// Do a few final mixes of the hash to ensure the last few
	// bytes are well-incorporated.

	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;

	return h;
}

static inline unsigned mh_str_hash(const char *kk) { return  mh_MurmurHash2(kk, strlen(kk), 13); }
#define mh_def_hash(key) ({						\
	unsigned ret;							\
	if (__builtin_types_compatible_p(mh_key_t, uint32_t) ||		\
	    __builtin_types_compatible_p(mh_key_t, int32_t))		\
		ret = (uintptr_t)(key);					\
	else if (__builtin_types_compatible_p(mh_key_t, uint64_t) ||	\
		 __builtin_types_compatible_p(mh_key_t, int64_t))	\
		ret = mh_u64_hash((uintptr_t)key);			\
	else if (__builtin_types_compatible_p(mh_key_t, char *) ||	\
		 __builtin_types_compatible_p(mh_key_t, const char *))	\
		ret = mh_str_hash((const char *)(uintptr_t)key);	\
	else								\
		assert("key type unmatched: provide your own mh_hash()" && NULL); \
	ret;								\
})

#define mh_def_eq(a, b) ({						\
	int ret;							\
	if (__builtin_types_compatible_p(mh_key_t, uint32_t) ||		\
	    __builtin_types_compatible_p(mh_key_t, int32_t) ||		\
	    __builtin_types_compatible_p(mh_key_t, uint64_t) ||		\
	    __builtin_types_compatible_p(mh_key_t, int64_t))		\
		ret = (a) == (b);					\
	else if (__builtin_types_compatible_p(mh_key_t, char *) ||	\
		 __builtin_types_compatible_p(mh_key_t, const char *))	\
		ret = !strcmp((const char *)(uintptr_t)(a), (const char *)(uintptr_t)(b)); \
	else								\
		assert("key type unmatched: provide your own mh_eq()" && NULL); \
	ret;								\
})

#endif

#ifndef mh_neighbors
#define mh_neighbors 4
#endif
#ifndef mh_may_skip
#if mh_byte_map > 0 && mh_neighbors > 1
#  define mh_may_skip 1
#else
#  define mh_may_skip 0
#endif
#endif
#if mh_neighbors == 1 && mh_may_skip
#undef mh_may_skip
#define mh_may_skip 0
#endif
#if mh_may_skip
#  define mh_need_dirty(loop)    ((loop).step & 1)
#  define mh_occupied_as_dirty(h, i)     (mh_dirty(h, (i)) || mh_dirty(h, (((i)+1) & h->n_mask)))
#  define mh_occupied_as_dirty_opt(h, i) (mh_dirty(h, (((i)+1) & h->n_mask)))
#else
#  define mh_need_dirty(loop)    1
#  define mh_occupied_as_dirty(h, i)     mh_dirty(h, (i))
#  define mh_occupied_as_dirty_opt(h, i) 0
#endif

struct _mh(find_loop) {
	unsigned i, inc, dlt, step;
};
/* original formula:
 * init:
 *   start_i = k & mask;
 *   dlt = k % (mask / (2 * mh_neighbors)) * 2 + 1;
 *   inc = 0
 *   i = start_i + inc;
 * step:
 *   inc = inc * 5 + dlt
 *   i = start_i + inc
 */
static inline void _mh(find_loop_init)(struct _mh(find_loop) *l, unsigned k, unsigned mask) {
	l->step = mh_neighbors;
	l->i = k & mask;
#if !MH_QUADRATIC_PROBING
#if mh_neighbors > 1
	l->dlt = (k % (mask / (2 * mh_neighbors))) * 2 * mh_neighbors;
#else
	l->dlt = (k % (mask / 2)) * 2 + 1;
#endif
#endif
	l->inc = 0;
}
static inline void _mh(find_loop_step)(struct _mh(find_loop) *l, unsigned mask) {
#ifdef mh_count_collisions
	mh_count_collisions;
#endif
#if mh_neighbors == 1
#  if MH_QUADRATIC_PROBING
	l->i += ++l->inc;
#  else
	uint32_t d = l->inc * 4 + l->dlt;
	l->i += d;
	l->inc += d;
#  endif
#else
	l->step--;
	l->i++;
	if (!l->step) {
#  if MH_QUADRATIC_PROBING
		l->step = mh_neighbors;
		l->i += l->inc;
		l->inc += mh_neighbors;
#  else
		uint32_t d = l->inc * 4 + l->dlt;
		l->step = mh_neighbors;
		l->i += d;
		l->inc += d + mh_neighbors;
#  endif
	}
#endif
	l->i &= mask;
}

static inline uint32_t
_mh(get)(const struct mhash_t *h, mh_key_t key)
{
	unsigned k = mh_hash(h, key);
	mh_map_t hk = mh_get_hashik(k);
	struct _mh(find_loop) l;
	_mh(find_loop_init)(&l, k, h->n_mask);
	for (;;) {
		if (mh_mayequal(h, l.i, hk) && mh_slot_key_eq(h, l.i, key))
			return l.i;

		if (mh_need_dirty(l) && !mh_dirty(h, l.i))
			return mh_end(h);

		_mh(find_loop_step)(&l, h->n_mask);
	}
}

static inline uint32_t
_mh(short_mark)(struct mhash_t *h, mh_key_t key)
{
	unsigned k = mh_hash(h, key);
	struct _mh(find_loop) l;
	_mh(find_loop_init)(&l, k, h->n_mask);
	for(;;) {
		if (mh_exist(h, l.i)) {
			if (mh_need_dirty(l)) mh_setdirty(h, l.i);
		} else {
			mh_map_t hk = mh_get_hashik(k);
			if (!mh_occupied_as_dirty(h, l.i))
				h->n_occupied++;
			mh_setexist(h, l.i, hk);
			h->size++;
			return l.i;
		}

		_mh(find_loop_step)(&l, h->n_mask);
	}
}

static inline uint32_t
_mh(mark)(struct mhash_t *h, mh_key_t key, int *exist)
{
	unsigned k = mh_hash(h, key), p = 0;
	mh_map_t hk = mh_get_hashik(k);
	struct _mh(find_loop) l;
	_mh(find_loop_init)(&l, k, h->n_mask);

	*exist = 1;
	do {
		if (mh_mayequal(h, l.i, hk) && mh_slot_key_eq(h, l.i, key)) {
			return l.i;
		}
		if (mh_exist(h, l.i)) {
			if (mh_need_dirty(l)) mh_setdirty(h, l.i);
		} else if (mh_need_dirty(l) && !mh_dirty(h, l.i)) {
			if (!mh_occupied_as_dirty_opt(h, l.i))
				h->n_occupied++;
			h->size++;
			mh_setexist(h, l.i, hk);
			*exist = 0;
			return l.i;
		} else {
			p = l.i+1;
		}

		_mh(find_loop_step)(&l, h->n_mask);
	} while (!p);

	for (;;) {
		if (mh_mayequal(h, l.i, hk) && mh_slot_key_eq(h, l.i, key)) {
#if MH_INCREMENTAL_RESIZE
			if (mh_unlikely(h->resize_position))
				return l.i;
#endif
			_mh(slot_copy)(h, p-1, mh_slot(h, l.i));
			mh_setfree(h, l.i);
			mh_setexist(h, p-1, hk);
			return p-1;
		}
		if (mh_need_dirty(l) && !mh_dirty(h, l.i)) {
			h->size++;
			if (!mh_occupied_as_dirty_opt(h, p-1))
				h->n_occupied++;
			mh_setexist(h, p-1, hk);
			*exist = 0;
			return p-1;
		}
		_mh(find_loop_step)(&l, h->n_mask);
	}
}

static inline void
_mh(resize_if_need)(struct mhash_t *h)
{
#if MH_INCREMENTAL_RESIZE
	if (mh_unlikely(h->resize_position))
		_mh(resize_step)(h);
	else
#endif
	if (mh_unlikely(h->n_occupied >= h->upper_bound))
		_mh(start_resize)(h, 0);
}

static inline uint32_t
_mh(iput)(struct mhash_t *h, mh_key_t key, int *ret)
{
	int exist;
	_mh(resize_if_need)(h);
	uint32_t x = _mh(mark)(h, key, &exist);
	if (!exist) {
		mh_slot_set_key(h, mh_slot(h, x), key);
	}

	if (ret)
		*ret = !exist;

#if MH_INCREMENTAL_RESIZE
	if (x < h->resize_position) {
		_mh(slot_copy_to_shadow)(h, x, exist);
	}
#endif
	return x;
}

static inline void
_mh(del)(struct mhash_t *h, uint32_t x)
{
	mh_setfree(h, x);
	h->size--;
	if (!mh_occupied_as_dirty(h, x))
		h->n_occupied--;

#if MH_INCREMENTAL_RESIZE
	if (mh_unlikely(h->resize_position)) {
		if (x < h->resize_position) {
			mh_key_t key = mh_slot_key(h, mh_slot(h, x)); /* mh_setfree() MUST keep key valid */
			struct mhash_t *s = h->shadow;
			uint32_t y = _mh(get)(s, key);
			assert(y != mh_end(s));
			_mh(del)(s, y);

			_mh(resize_step)(h);
		}
	}
#endif
}

/* slot variants */
static inline uint32_t
_mh(sget_by_key)(const struct mhash_t *h, mh_key_t key)
{
	return _mh(get)(h, key);
}

static inline uint32_t
_mh(sget)(const struct mhash_t *h, mh_slot_t const *slot)
{
	return _mh(get)(h, mh_slot_key(h, slot));
}

static inline int
_mh(sput)(struct mhash_t *h, mh_slot_t const *slot, mh_slot_t *prev_slot)
{
	int exist;
	_mh(resize_if_need)(h);
	uint32_t x = _mh(mark)(h, mh_slot_key(h, slot), &exist);

	if (exist && prev_slot)
		mh_slot_copy(d, prev_slot, mh_slot(h, x));

	_mh(slot_copy)(h, x, slot); /* always copy: overwrite old slot val if exists */
#if MH_INCREMENTAL_RESIZE
	if (x < h->resize_position)
		_mh(slot_copy_to_shadow)(h, x, exist);
#endif

	return !exist;
}

static inline int
_mh(sremove_by_key)(struct mhash_t *h, mh_key_t key, mh_slot_t *prev_slot)
{
	uint32_t x = _mh(get)(h, key);
	int exist = x != mh_end(h);
	if (exist) {
		if (prev_slot)
			mh_slot_copy(d, prev_slot, mh_slot(h, x));

		_mh(del)(h, x);
	}
	return exist;
}
static inline int
_mh(sremove)(struct mhash_t *h, mh_slot_t const *slot, mh_slot_t *prev_slot)
{
	return _mh(sremove_by_key)(h, mh_slot_key(h, slot), prev_slot);
}

static inline mh_key_t _mh(key)(struct mhash_t *h, uint32_t x)
{
	return mh_slot_key(h, mh_slot(h, x));
}

#ifdef mh_val_t
static inline int
_mh(put)(struct mhash_t *h, mh_key_t key, mh_val_t val, mh_val_t *prev_val)
{
	int exist;
	_mh(resize_if_need)(h);
	uint32_t x = _mh(mark)(h, key, &exist);
	mh_slot_t *slot = mh_slot(h, x);
	if (!exist) {
		mh_slot_set_key(h, slot, key);
	}

	if (exist && prev_val)
		*prev_val = mh_slot_val(slot);

#ifndef mh_slot_set_val
	memcpy(&mh_slot_val(slot), &(val), sizeof(mh_val_t));
#else
	mh_slot_set_val(slot, val);
#endif
#if MH_INCREMENTAL_RESIZE
	if (x < h->resize_position)
		_mh(slot_copy_to_shadow)(h, x, exist);
#endif

	return !exist;
}

static inline void
_mh(set_value)(struct mhash_t *h, uint32_t x, mh_val_t val)
{
	mh_slot_t *slot = mh_slot(h, x);
#ifndef mh_slot_set_val
	memcpy(&mh_slot_val(slot), &(val), sizeof(mh_val_t));
#else
	mh_slot_set_val(slot, val);
#endif

#if MH_INCREMENTAL_RESIZE
	if (x < h->resize_position)
		_mh(slot_copy_to_shadow)(h, x, 1);
#endif
}

static inline mh_val_t
_mh(value)(struct mhash_t *h, uint32_t x)
{
	return mh_slot_val(mh_slot(h, x));
}

static inline mh_val_t MH_INCREMENTAL_CONST *
_mh(pvalue)(struct mhash_t *h, uint32_t x)
{
	return &mh_slot_val(mh_slot(h, x));
}

static inline int
_mh(remove)(struct mhash_t *h, mh_key_t key, mh_val_t *prev_val)
{
	uint32_t x = _mh(get)(h, key);
	if (x != mh_end(h)) {
		if (prev_val)
			*prev_val = mh_slot_val(mh_slot(h, x));

		_mh(del)(h, x);
	}
	return x != mh_end(h);
}
#endif

static inline int
_mh(exist)(struct mhash_t *h, mh_key_t key)
{
	uint32_t k;

	k = _mh(get)(h, key);
	return (k != mh_end(h));
}


static inline int
_mh(slot_occupied)(struct mhash_t *h, uint32_t x)
{
	return mh_exist(h, x);
}

static inline void
_mh(slot_copy)(struct mhash_t *d, uint32_t dx, mh_slot_t const *source)
{
	mh_slot_copy(d, mh_slot(d, dx), source);
}

/* This is hijack method. Use it only if you really want to destroy hash and you know what are you doing */
static inline void
_mh(hijack_slot_put)(struct mhash_t *d, uint32_t dx, mh_slot_t const *source)
{
	mh_slot_copy(d, mh_slot(d, dx), source);
	mh_setexist(d, dx, 2);
}

/* This is hijack method. Use it only if you really want to destroy hash and you know what are you doing */
static inline void
_mh(hijack_slot_free)(struct mhash_t *d, uint32_t dx)
{
	mh_setfree(d, dx);
}

#ifdef MH_SOURCE

#ifndef load_factor
#define load_factor 0.73
#endif

MH_DECL struct mhash_t *
_mh(init)(void *(*custom_realloc)(void *, size_t))
{
	custom_realloc = custom_realloc ?: realloc;
	struct mhash_t *h = custom_realloc(NULL, sizeof(*h));
	memset(h, 0, sizeof(*h));
	h->realloc = custom_realloc;
	_mh(initialize)(h);
	return h;
}

MH_DECL void
_mh(initialize)(struct mhash_t *h)
{
	h->realloc = h->realloc ?: realloc;
	h->node_size = h->node_size ?: sizeof(mh_slot_t);

	h->shadow = mh_calloc(h, 1, sizeof(*h));
	h->n_mask = mh_neighbors * 4 - 1;

	h->slots = mh_calloc(h, mh_end(h), mh_slot_size(h));
#ifndef mh_custom_map
#if mh_byte_map
	h->map = mh_calloc(h, mh_end(h), sizeof(mh_map_t)); /* 4 maps per char */
#else
	h->map = mh_calloc(h, (mh_end(h) + 15) / 16, 4); /* 4 maps per char */
#endif
#endif
	h->size = 0;
	h->n_occupied = 0;
	h->upper_bound = h->n_mask * load_factor;
	h->resize_position = 0;
	h->resize_batch = 0;
}

MH_DECL void
_mh(slot_move)(struct mhash_t *h, uint32_t dx, uint32_t sx)
{
	_mh(slot_copy)(h, dx, mh_slot(h, sx));
	mh_setfree(h, sx);
}


MH_DECL void
_mh(resize_step)(struct mhash_t *h)
{
	struct mhash_t *s = h->shadow;
	uint32_t start = h->resize_position,
		   end = mh_end(h);

#if MH_INCREMENTAL_RESIZE
	uint32_t batch_end = h->resize_position + h->resize_batch;
	if (batch_end < end)
		end = batch_end;

	h->resize_position += h->resize_batch;
#endif
	uint32_t o;
	for (o = start; o < end; o++) {
		if (mh_exist(h, o)) {
			mh_slot_t *slot = mh_slot(h, o);
			uint32_t n = _mh(short_mark)(s, mh_slot_key(s, slot));
			_mh(slot_copy)(s, n, slot);
		}
	}

	if (end == mh_end(h)) {
		assert(s->size == h->size);
		mh_free(h, h->slots);
#ifndef mh_custom_map
		mh_free(h, h->map);
#endif
		memcpy(h, s, sizeof(*h));
		memset(s, 0, sizeof(*s));
	}
}

MH_DECL void
_mh(slot_copy_to_shadow)(struct mhash_t *h, uint32_t o, int exist)
{
	struct mhash_t *s = h->shadow;

	mh_slot_t *slot = mh_slot(h, o);
	if (exist) {
		uint32_t n = _mh(get)(s, mh_slot_key(s, slot));
		_mh(slot_copy)(s, n, slot);
	} else {
		_mh(sput)(s, slot, NULL);
	}
}

MH_DECL void
_mh(start_resize)(struct mhash_t *h, uint32_t want_size)
{
	if (h->resize_position)
		return;
	struct mhash_t *s = h->shadow;
	uint32_t upper_bound;
	uint32_t n_buckets;

	if (h->size + 1 > want_size) want_size = h->size + 1;
	if ((double)want_size < (1u << 31) * (load_factor * 0.84)) {
		want_size = want_size / (load_factor * 0.85) + 1;
	} else if (want_size < (1u << 31)) {
		want_size = (1u << 31);
	} else {
		abort();
	}
	n_buckets = mh_neighbors * 4;
	while (n_buckets < want_size) n_buckets *= 2;
	if (n_buckets < (1u << 31)) {
		upper_bound = n_buckets * load_factor;
	} else {
		upper_bound = h->upper_bound + (want_size - h->upper_bound) / 2;
	}

#if MH_INCREMENTAL_RESIZE
	h->resize_batch = 64;
#endif
	memcpy(s, h, sizeof(*h));
	s->resize_position = 0;
	s->n_mask = n_buckets - 1;
	s->upper_bound = upper_bound;
	s->n_occupied = 0;
	s->size = 0;
	s->slots = mh_calloc(h, (size_t)mh_end(s), mh_slot_size(h));
#ifndef mh_custom_map
#if mh_byte_map
	s->map = mh_calloc(h, mh_end(s), sizeof(mh_map_t)); /* 4 maps per char */
#else
	s->map = mh_calloc(h, (mh_end(s) + 15) / 16, 4); /* 4 maps per char */
#endif
#endif

	_mh(resize_step)(h);
}

MH_DECL size_t
_mh(bytes)(struct mhash_t *h)
{
	return h->resize_position ? _mh(bytes)(h->shadow) : 0 +
		sizeof(*h) +
		((size_t)mh_end(h)) * mh_slot_size(h) +
#ifndef mh_custom_map
#if mh_byte_map == 0
		((size_t)h->n_mask / 16 + 1) *  sizeof(uint32_t)
#elif mh_byte_map == 1
		(size_t)h->n_mask
#elif mh_byte_map == 2
		(size_t)h->n_mask * 2
#endif
#else
		0
#endif
		;

}

MH_DECL void
_mh(clear)(struct mhash_t *h)
{
	_mh(destruct)(h);
	_mh(initialize)(h);
}

MH_DECL void
_mh(destruct)(struct mhash_t *h)
{
#ifdef MH_INCREMENTAL_RESIZE
	if (h->shadow->slots) {
		mh_free(h, h->shadow->slots);
#ifndef mh_custom_map
		mh_free(h, h->shadow->map);
#endif
	}
#endif
	mh_free(h, h->shadow);
#ifndef mh_custom_map
	mh_free(h, h->map);
#endif
	mh_free(h, h->slots);
}

MH_DECL void
_mh(destroy)(struct mhash_t *h)
{
	_mh(destruct)(h);
	mh_free(h, h);
}

#ifdef MH_DEBUG
#include <stdio.h>
MH_DECL void
_mh(dump)(struct mhash_t *h)
{
	printf("slots:\n");
	int k = 0, i = 0;
	for(i = 0; i < mh_end(h); i++) {
		if (mh_dirty(h, i) || mh_exist(h, i)) {
			printf("   [%i] ", i);
			if (mh_exist(h, i)) {
				printf("   -> %s", h->slots[i].key);
				k++;
			}
			if (mh_dirty(h, i))
				printf(" dirty");
			printf("\n");
		}
	}
	printf("end(%i)\n", k);
}
#  endif
#endif

#undef mh_key_t
#undef mh_val_t
#undef mh_slot_t

#undef mh_slot
#undef mh_slot_key
#undef mh_slot_val
#undef mh_slot_size

#undef mh_hash
#undef mh_eq
#undef mh_slot_key_eq
#undef mh_slot_set_key
#undef mh_slot_copy

#undef mh_arg_t

#undef mh_byte_map
#undef mh_may_skip
#undef mh_need_dirty
#undef mh_occupied_as_dirty
#undef mh_occupied_as_dirty_opt
#undef mh_neighbors
#undef mh_exist
#undef mh_map_t
#undef mh_divider
#undef mh_get_hashik
#undef mh_mayequal
#undef mh_setfree
#undef mh_setexist
#undef mh_dirty
#undef mh_setdirty

#undef mh_malloc
#undef mh_calloc
#undef mh_free

#undef mh_name
#undef mhash_t
#undef MH_DECL
