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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "vsc_head.h"
#include "vsc_obj.h"


/*
 * VSC Json
 */

static char dyn_vsc_json_fmt[] = "{"
	"  \"version\": \"1\","
	"  \"name\": \"%s\","
	"  \"oneliner\": \"Dynamic metrics\","
	"  \"order\": 200,"
	"  \"docs\": \"\","
	"  \"elements\": 1,"
	"  \"elem\": {"
	"    \"val\": {"
	"      \"ctype\": \"uint64_t\","
	"      \"format\": \"%s\","
	"      \"type\": \"%s\","
	"      \"level\": \"%s\","
	"      \"index\": 0,"
	"      \"name\": \"%s\","
	"      \"oneliner\": \"%s\","
	"      \"docs\": \"\""
	"    }"
	"  }"
	"}";

static void
dyn_json(VCL_STRING class_name, VCL_STRING name, VCL_STRING format, VCL_STRING type, VCL_STRING level, VCL_STRING oneliner, char **s, size_t *l)
{
	size_t t = 0;
	char *json;

	t += strlen(class_name);
	t += strlen(format);
	t += strlen(type);
	t += strlen(level);
	t += strlen(name);
	t += strlen(oneliner);
	t += sizeof dyn_vsc_json_fmt;

	json = calloc(t, 1);
	snprintf(json, t, dyn_vsc_json_fmt, class_name, format, type, level, name, oneliner);
	*s = json;
	*l = t;
}

struct vmod_dyncounters_doc *
vmod_dyncounters_add_doc(struct vmod_dyncounters_head *head, VCL_STRING suffix, VCL_ENUM format, VCL_ENUM type, VCL_ENUM level, VCL_STRING oneliner, int lock)
{
	struct vmod_dyncounters_doc *d;
	if (lock)
		AZ(pthread_mutex_lock(&head->doc_mtx));
	VTAILQ_FOREACH(d, &head->doc, list) {
		CHECK_OBJ_NOTNULL(d, VMOD_DYN_DOC_MAGIC);
		if (strcmp(d->name, suffix) == 0) {
			if (lock)
				AZ(pthread_mutex_unlock(&head->doc_mtx));
			return (d);
		}
	}
	if (lock) {
		ALLOC_OBJ(d, VMOD_DYN_DOC_MAGIC);
		AN(d);
		d->name = strdup(suffix);
		dyn_json(head->name, d->name, format, type, level, oneliner, &d->json, &d->json_size);
		VTAILQ_INSERT_TAIL(&head->doc, d, list);
		AZ(pthread_mutex_unlock(&head->doc_mtx));
		return (d);
	}
	return vmod_dyncounters_add_doc(head, suffix, format, type, level, oneliner, 1);
}

void
vmod_dyncounters_free_doc(struct vmod_dyncounters_head *h)
{
	struct vmod_dyncounters_doc *d, *t;

	VTAILQ_FOREACH_SAFE(d, &h->doc, list, t) {
		VTAILQ_REMOVE(&h->doc, d, list);
		free(d->json);
		free(d->name);
		FREE_OBJ(d);
	}
}

/*
 * VSC cluster
 */

struct vmod_dyncounters_vsm *
vmod_dyncounters_get_vsm(VRT_CTX, struct vmod_dyncounters_head *head, int lock)
{
	struct vmod_dyncounters_vsm *m;
	int slot, nbp = 1;
	if (lock)
		AZ(pthread_mutex_lock(&head->vsm_mtx));
	VTAILQ_FOREACH(m, &(head->vsm), list) {
		CHECK_OBJ_NOTNULL(m, VMOD_DYN_VSM_MAGIC);
		slot = m->slot;
		if (slot > 0) {
			slot = __sync_sub_and_fetch(&m->slot, 1);
			if (slot >= 0) {
				if (lock)
					AZ(pthread_mutex_unlock(&head->vsm_mtx));
				return (m);
			}
			__sync_fetch_and_add(&m->slot, 1);
		}
		nbp++;
	}
	AZ(m);
	if (lock) {
		size_t vo = vmod_dyncounters_vsc_size();
		ALLOC_OBJ(m, VMOD_DYN_VSM_MAGIC);
		AN(m);
		/* in absolute vsc_overhead=32, VSM_CLUSTER_OFFSET=16 (take vo as value) -> 128 slot per page - 1 */
		/* nb pages is up to 1 for each entry in vsm list */
		AN(nbp);
		slot = (((size_t)getpagesize() * nbp - vo) / vo);
		m->vsc_cluster = VRT_VSM_Cluster_New(ctx, slot * vo);
		m->slot = slot - 1;
		VTAILQ_INSERT_HEAD(&head->vsm, m, list);
		AZ(pthread_mutex_unlock(&head->vsm_mtx));
		return (m);
	}
	return vmod_dyncounters_get_vsm(ctx, head, 1);
}

void
vmod_dyncounters_free_vsm(VRT_CTX, struct vmod_dyncounters_head *h)
{
	struct vmod_dyncounters_vsm *m, *t;

	VTAILQ_FOREACH_SAFE(m, &h->vsm, list, t) {
		VTAILQ_REMOVE(&h->vsm, m, list);
		VRT_VSM_Cluster_Destroy(ctx, &m->vsc_cluster);
		FREE_OBJ(m);
	}
}
