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

#include <cache/cache.h>
#include "vsc_head.h"
#include "vsc_obj.h"
#include "vcc_dyncounters_if.h"

#include <stdlib.h>
#include <string.h>

/*
  dynamic VSC cross VCL

  "<vcl_name>.<dynname>.<suffix>"

 */

static VTAILQ_HEAD(,vmod_dyncounters_head) vmod_dyncounters_pool =
  VTAILQ_HEAD_INITIALIZER(vmod_dyncounters_pool);

VCL_VOID
vmod_head_incr(VRT_CTX, struct vmod_dyncounters_head *h, VCL_STRANDS radical, VCL_STRING suffix, VCL_INT d)
{
	vmod_dyn_vsc_incr(ctx, h, radical, suffix, d);
}

VCL_VOID
vmod_head_decr(VRT_CTX, struct vmod_dyncounters_head *h, VCL_STRANDS radical, VCL_STRING suffix, VCL_INT d)
{
	vmod_dyn_vsc_decr(ctx, h, radical, suffix, d);
}

VCL_VOID
vmod_head_set(VRT_CTX, struct vmod_dyncounters_head *h, VCL_STRANDS radical, VCL_STRING suffix, VCL_INT d)
{
	vmod_dyn_vsc_set(ctx, h, radical, suffix, d);
}

VCL_VOID
vmod_head_doc(VRT_CTX, struct vmod_dyncounters_head *h, VCL_STRING suffix, VCL_ENUM format, VCL_ENUM type, VCL_ENUM level, VCL_STRING oneliner)
{
	CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
	vmod_dyncounters_add_doc(h, suffix, format, type, level, oneliner, 0);
}

VCL_VOID
vmod_head__init(VRT_CTX, struct vmod_dyncounters_head **ph, const char *vcl_name)
{
	struct vmod_dyncounters_head *h;

	CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
	VTAILQ_FOREACH(h, &vmod_dyncounters_pool, list) {
		CHECK_OBJ_NOTNULL(h, VMOD_DYN_HEAD_MAGIC);
		if (strcmp(h->name, vcl_name) == 0)
			break;
	}
	if (!h) {
		ALLOC_OBJ(h, VMOD_DYN_HEAD_MAGIC);
		AN(h);
		h->name = strdup(vcl_name);
		AZ(pthread_mutex_init(&h->doc_mtx, NULL));
		AZ(pthread_mutex_init(&h->vsm_mtx, NULL));
		VTAILQ_INIT(&(h->doc));
		VTAILQ_INIT(&(h->vsm));
		VTAILQ_INSERT_HEAD(&vmod_dyncounters_pool, h, list);
	}
	h->load += 1;
	*ph = h;
}

VCL_VOID
vmod_head__fini(struct vmod_dyncounters_head **ph)
{
	struct vmod_dyncounters_head *h;

	TAKE_OBJ_NOTNULL(h, ph, VMOD_DYN_HEAD_MAGIC);
	h->load -= 1;
}

int v_matchproto_(vmod_event_f)
vmod_dyncounters_event(VRT_CTX, struct vmod_priv *priv, enum vcl_event_e e)
{
	if (e == VCL_EVENT_DISCARD) {
		struct vmod_dyncounters_head *h, *t;
		VTAILQ_FOREACH_SAFE(h, &vmod_dyncounters_pool, list, t) {
			CHECK_OBJ_NOTNULL(h, VMOD_DYN_HEAD_MAGIC);
			if (h->load)
				continue;
			VTAILQ_REMOVE(&vmod_dyncounters_pool, h, list);
			vmod_dyn_vsc_free_all(h);
			vmod_dyncounters_free_vsm(ctx, h);
			vmod_dyncounters_free_doc(h);
			AZ(pthread_mutex_destroy(&h->doc_mtx));
			AZ(pthread_mutex_destroy(&h->vsm_mtx));
			free(h->name);
			FREE_OBJ(h);
		}
	}
	return (0);
}
