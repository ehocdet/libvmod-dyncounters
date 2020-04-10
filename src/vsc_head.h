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

struct vmod_dyncounters_doc {
	unsigned		magic;
#define VMOD_DYN_DOC_MAGIC		0x62599755
	char       		*name;
	char       		*json;
	size_t     		json_size;
	VTAILQ_ENTRY(vmod_dyncounters_doc) list;
};

struct vmod_dyncounters_vsm {
	unsigned		magic;
#define VMOD_DYN_VSM_MAGIC		0xb679f996
	int		slot;
	struct vsmw_cluster     *vsc_cluster;
	VTAILQ_ENTRY(vmod_dyncounters_vsm) list;
};

struct vmod_dyncounters_head {
	unsigned		magic;
#define VMOD_DYN_HEAD_MAGIC		0xd301f98f
	unsigned		load;
	char			*name;
	pthread_mutex_t		doc_mtx;
        VTAILQ_HEAD(,vmod_dyncounters_doc) doc;
	pthread_mutex_t		vsm_mtx;
        VTAILQ_HEAD(vscmemhead,vmod_dyncounters_vsm) vsm;
	lft_root                root;
	VTAILQ_ENTRY(vmod_dyncounters_head) list;
};

struct vmod_dyncounters_doc * vmod_dyncounters_add_doc(struct vmod_dyncounters_head *, VCL_STRING name, VCL_ENUM format, VCL_ENUM type, VCL_ENUM level, VCL_STRING oneliner, int);
struct vmod_dyncounters_vsm * vmod_dyncounters_get_vsm(VRT_CTX, struct vmod_dyncounters_head *, int);
void vmod_dyncounters_free_doc(struct vmod_dyncounters_head *);
void vmod_dyncounters_free_vsm(VRT_CTX, struct vmod_dyncounters_head *);
