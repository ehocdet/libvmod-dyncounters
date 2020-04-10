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
 */

#include "lft.h"

typedef uintptr_t lft_obj_alloc_f(const uint8_t*, uintptr_t);
typedef void lft_obj_free_f(uintptr_t, uintptr_t);
typedef int lft_obj_check_f(uintptr_t);
typedef const uint8_t* lft_obj_get_digest_f(uintptr_t);

struct lft_obj_func {
	lft_obj_alloc_f         *alloc;
	lft_obj_free_f          *free;
	lft_obj_check_f         *check;
	lft_obj_get_digest_f    *get_digest;
	unsigned char		digest_len;
};

uintptr_t lft_insert(lft_root *, const uint8_t *, struct lft_obj_func *, uintptr_t *, uintptr_t);
void lft_free(lft_root *, struct lft_obj_func *, uintptr_t);
