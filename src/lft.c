/*-
 * Copyright (c) 2020 Emmanuel Hocdet
 * All rights reserved.
 *
 * Author: Emmanuel Hocdet <ehocdet@club.fr>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
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
 *
 * A lock free critbit based tree with discret external node.
 * Code is widely inspired by Varnish hash_cribit.c
 */


#include "config.h"
#include "cache/cache.h"
#include "lft.h"
#include "lft_obj.h"

#include <stdlib.h>
#include <string.h>


/*
 * Table for finding out how many bits two bytes have in common,
 * counting from the MSB towards the LSB.
 * ie:
 *	lft_bittbl[0x01 ^ 0x22] == 2
 *	lft_bittbl[0x10 ^ 0x0b] == 3
 *
 */

static unsigned char lft_bittbl[256] = {
  8,7,6,6,5,5,5,5,4,4,4,4,4,4,4,4,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

static inline unsigned char
lft_bits(unsigned char x, unsigned char y)
{
	return (lft_bittbl[x ^ y]);
}

/*
 * Y (internal node)
 */

struct lft_y {
	unsigned		magic;
#define LFT_Y_MAGIC		0x125c4bd2
	unsigned short		critbit;
	unsigned char		ptr;
	unsigned char		bitmask;
	volatile uintptr_t	leaf[2];
};

#define LFT_BIT_NODE		(1<<0)
#define LFT_BIT_Y		(1<<1)

static inline int
lft_is_y(uintptr_t u)
{
	return (u & LFT_BIT_Y);
}

static inline struct lft_y *
lft_l_y(uintptr_t u)
{
	assert(u & LFT_BIT_Y);
	return ((struct lft_y *)(u & ~LFT_BIT_Y));
}

static inline uintptr_t
lft_r_y(const struct lft_y *y)
{
	CHECK_OBJ_NOTNULL(y, LFT_Y_MAGIC);
	return (LFT_BIT_Y | (uintptr_t)y);
}

static inline int
lft_is_node(uintptr_t u)
{
	return (u & LFT_BIT_NODE);
}

static inline uintptr_t
lft_l_node(uintptr_t u)
{
	assert(u & LFT_BIT_NODE);
	return (u & ~LFT_BIT_NODE);
}

static inline uintptr_t
lft_r_node(uintptr_t u)
{
	return (LFT_BIT_NODE | u);
}

/*
 * Find the "critical" bit that separates these two digests
 */

static void
lft_crit_bit(const uint8_t *digest, const uint8_t *digest2, unsigned char digest_len, struct lft_y *y)
{
	unsigned char u, r;

	CHECK_OBJ_NOTNULL(y, LFT_Y_MAGIC);
	for (u = 0; u < digest_len && digest[u] == digest2[u]; u++)
		;
	assert(u < digest_len);
	r = lft_bits(digest[u], digest2[u]);
	y->ptr = u;
	y->bitmask = 0x80 >> r;
	y->critbit = u * 8 + r;
}

/*
 * Lookup/Create/Insert
 */

uintptr_t
lft_insert(lft_root *root, const uint8_t *digest, struct lft_obj_func *func, uintptr_t *op, uintptr_t args)
{
	volatile uintptr_t *p;
	uintptr_t pp;
	uintptr_t o2;
	struct lft_y *y, *y2;
	unsigned s, s2;

	assert(op != NULL);
	assert(!*op || func->check(*op));

	p = root;
	pp = *p;
	/* First */
	if (pp == 0) {
		if (!*op)
			*op = func->alloc(digest, args);
		o2 = *op;
		if (__sync_bool_compare_and_swap(p, pp, lft_r_node(o2)))
			return (o2);
		return (0);
	}
	/* Seek */
	while (lft_is_y(pp)) {
		y = lft_l_y(pp);
		assert(y->ptr < func->digest_len);
		s = (digest[y->ptr] & y->bitmask) != 0;
		assert(s < 2);
		p = &y->leaf[s];
		pp = *p;
	}

	assert(pp); // XXX
	if (pp == 0) {
		/* via lft_remove (not implemented) */
		return (0);
	}

	/* We found a node, does it match ? */
	assert(lft_is_node(pp));
	o2 = lft_l_node(pp);
	assert(func->check(o2));
	if (!memcmp(digest, func->get_digest(o2), func->digest_len))
		return (o2);

	/* Create */
	ALLOC_OBJ(y2, LFT_Y_MAGIC);
	AN(y2);
	lft_crit_bit(digest, func->get_digest(o2), func->digest_len, y2);
	s2 = (digest[y2->ptr] & y2->bitmask) != 0;
	assert(s2 < 2);
	if (!*op)
		*op = func->alloc(digest, args);
	o2 = *op;
	y2->leaf[s2] = lft_r_node(o2);
	s2 = 1-s2;

	/* Insert */
	p = root;
	pp = *p;
	while (lft_is_y(pp)) {
		y = lft_l_y(pp);
		assert(y->critbit != y2->critbit);
		if (y->critbit > y2->critbit)
			break;
		assert(y->ptr < func->digest_len);
		s = (digest[y->ptr] & y->bitmask) != 0;
		assert(s < 2);
		p = &y->leaf[s];
		pp = *p;
	}
	y2->leaf[s2] = pp;
	if (__sync_bool_compare_and_swap(p, pp, lft_r_y(y2)))
		return (o2);
	/* Abort */
	FREE_OBJ(y2);
	return (0);
}

/*
 * Free all
 */

static void
_lft_free(uintptr_t p, struct lft_obj_func *func, uintptr_t args)
{
	if (p == 0)
		return;
	if (lft_is_node(p)) {
		uintptr_t o = lft_l_node(p);
		assert(func->check(o));
		func->free(o, args);
		return;
	}
	if (lft_is_y(p)) {
		struct lft_y *y = lft_l_y(p);
		_lft_free(y->leaf[0], func, args);
		_lft_free(y->leaf[1], func, args);
		FREE_OBJ(y);
		return;
	}
	assert(0);
}

void
lft_free(lft_root *root, struct lft_obj_func *func, uintptr_t args)
{
	_lft_free(*root, func, args);
	*root = 0;
}
