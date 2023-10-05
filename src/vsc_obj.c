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

#include "config.h"

#include "cache/cache.h"
#include "vsha256.h"

#include "vsc_head.h"
#include "lft_obj.h"
#include "vsc_obj.h"

#include <stdlib.h>
#include <string.h>

struct VSC_value {
	uint64_t	value;
};

struct vmod_dyn_vsc {
	unsigned		magic;
#define VMOD_DYN_VSC_MAGIC		0x038e96d4
	uint8_t			digest[VSHA256_LEN];
	struct vsc_seg		*vsc_seg;
	struct VSC_value	*vsc;
};

struct dyn_vsc_args {
	struct vmod_dyncounters_head *head;
	struct vmod_dyncounters_vsm *vsm;
	VCL_STRANDS            	radical;
	VCL_STRING            	suffix;
	VRT_CTX                 ;
};

#define VSC_value_size (sizeof(struct VSC_value))

size_t
vmod_dyncounters_vsc_size()
{
	return (VRT_VSC_Overhead(VSC_value_size));
}

/*
 * lft_obj_func instantiation
 */

static int v_matchproto_(lft_obj_check_f)
obj_check(uintptr_t u)
{
	struct vmod_dyn_vsc *o;
	o = (struct vmod_dyn_vsc *) u;
	return (u ? o->magic == VMOD_DYN_VSC_MAGIC : 0);
}

static const uint8_t* v_matchproto_(lft_obj_get_digest_f)
obj_get_digest(uintptr_t u)
{
	struct vmod_dyn_vsc *o;
	CAST_OBJ_NOTNULL(o, (void *)u, VMOD_DYN_VSC_MAGIC);
	return (o->digest);
}

static void
obj_vsc_alloc(struct vmod_dyn_vsc *o, struct dyn_vsc_args *args, const char *fmt, ...)
{
	struct vmod_dyncounters_doc *d;
	struct vmod_dyncounters_vsm *m;
	const char *p;
	va_list ap;

	/* Same format/type/level/oneliner as vmod_head_doc default */
	d = vmod_dyncounters_add_doc(args->head, args->suffix, "integer", "counter", "info", "", 0);
	m = vmod_dyncounters_get_vsm(args->ctx, args->head, 0);
	args->vsm = m;

	va_start(ap, fmt);
	o->vsc = VRT_VSC_Alloc(m->vsc_cluster, &o->vsc_seg, args->head->name, VSC_value_size, d->json, d->json_size, fmt, ap);
	/* XXX VSC could be visible and duplicate, vanish-cache issue */
	VRT_VSC_Hide(o->vsc_seg);
	va_end(ap);
}

static uintptr_t v_matchproto_(lft_obj_alloc_f)
obj_alloc(const uint8_t *digest, uintptr_t ua)
{
	struct dyn_vsc_args *args;
	struct vmod_dyn_vsc *o;
	VCL_STRING p;
        uintptr_t sn;

	AN(ua);
	args = (struct dyn_vsc_args *) ua;
	ALLOC_OBJ(o, VMOD_DYN_VSC_MAGIC);
	AN(o);
	memcpy(o->digest, digest, sizeof o->digest);

        CHECK_OBJ_NOTNULL(args->ctx, VRT_CTX_MAGIC);
	/* Strands to va_list via string */
        sn = WS_Snapshot(args->ctx->ws);
        p = VRT_StrandsWS(args->ctx->ws, NULL, args->radical);
        if (p)
		obj_vsc_alloc(o, args, p);
	else {
		FREE_OBJ(o);
                VRT_fail(args->ctx, "Workspace overflow");
	}
        WS_Reset(args->ctx->ws, sn);

	return ((uintptr_t)o);
}

static void v_matchproto_(lft_obj_free_f)
obj_free(uintptr_t u,  uintptr_t ua)
{
	struct vmod_dyn_vsc *o;
	AN(ua);
	CAST_OBJ_NOTNULL(o, (void *)u, VMOD_DYN_VSC_MAGIC);
	VRT_VSC_Destroy((char *) ua, o->vsc_seg);
	FREE_OBJ(o);
}

struct lft_obj_func vsc_obj_func = {
	.digest_len = VSHA256_LEN,
	.check = obj_check,
	.get_digest = obj_get_digest,
	.alloc = obj_alloc,
	.free = obj_free,
};

/* call from vsc_head */

static struct vmod_dyn_vsc *
obj_insert(VRT_CTX, struct vmod_dyncounters_head *head, VCL_STRANDS radical, VCL_STRING suffix)
{
	struct vmod_dyn_vsc *o = NULL;
	struct vmod_dyn_vsc *o2;
	struct dyn_vsc_args args;
	struct VSHA256Context sha_ctx;
	const char *p;
	uint8_t sha256[VSHA256_LEN];
	int i;

	/* digest */
	VSHA256_Init(&sha_ctx);
	for (i = 0; i < radical->n; i++) {
		p = radical->p[i];
		if (p != NULL && *p != '\0')
			VSHA256_Update(&sha_ctx, p, strlen(p));
	}
	VSHA256_Update(&sha_ctx, suffix, strlen(suffix));
        VSHA256_Final(sha256, &sha_ctx);

	/* args for obj_alloc */
	args.head = head;
	args.radical = radical;
	args.suffix = suffix;
	args.ctx = ctx;
	args.vsm = NULL;

	pthread_mutex_lock(&head->insert_mtx);
	while (!(o2 = (struct vmod_dyn_vsc *) lft_insert(&head->root, sha256, &vsc_obj_func, (uintptr_t *) &o, (uintptr_t) &args)))
	;
	pthread_mutex_unlock(&head->insert_mtx);

	CHECK_OBJ_NOTNULL(o2, VMOD_DYN_VSC_MAGIC);
	if (o == o2) {
		/* new obj */
		VRT_VSC_Reveal(o->vsc_seg);
	} else if (o) {
		/* duplicate (obj_free) */
		VRT_VSC_Destroy(head->name, o->vsc_seg);
		FREE_OBJ(o);
		/* vsc_cluster slot free */
		AN(args.vsm);
		__sync_fetch_and_add(&args.vsm->slot, 1);
	}

	return (o2);
}

void
vmod_dyn_vsc_free_all(struct vmod_dyncounters_head *h)
{
	lft_free(&h->root, &vsc_obj_func, (uintptr_t) h->name);
}

void
vmod_dyn_vsc_incr(VRT_CTX, struct vmod_dyncounters_head *h, VCL_STRANDS radical, VCL_STRING suffix, VCL_INT d)
{
	struct vmod_dyn_vsc *o;
	if (d < 0)
		return;
	o = obj_insert(ctx, h, radical, suffix);
	__sync_fetch_and_add(&o->vsc->value, d);
}

void
vmod_dyn_vsc_decr(VRT_CTX, struct vmod_dyncounters_head *h, VCL_STRANDS radical, VCL_STRING suffix, VCL_INT d)
{
	struct vmod_dyn_vsc *o;
	if (d < 0)
		return;
	o = obj_insert(ctx, h, radical, suffix);
	__sync_fetch_and_sub(&o->vsc->value, d);
}

void
vmod_dyn_vsc_set(VRT_CTX, struct vmod_dyncounters_head *h, VCL_STRANDS radical, VCL_STRING suffix, VCL_INT d)
{
	struct vmod_dyn_vsc *o;
	if (d < 0)
		return;
	o = obj_insert(ctx, h, radical, suffix);
	o->vsc->value = d;
}
