/*
 * Copyright (C) 2008 The Android Open Source Project
 * All rights reserved.
 *
 *Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the 
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <list.h>
#include <stdlib.h>
#include <string.h>
#include <lib/ptable.h>

void ptable_init(struct ptable *ptable)
{
	ASSERT(ptable);
	memset(ptable, 0, sizeof(struct ptable));
}

char* ptype[] = {"Apps", "Modem"};
char* pperm[] = {"No", "Yes"};

void ptable_add(struct ptable *ptable, char *name, unsigned start,
		unsigned length, unsigned flags, char type, char perm)
{
	struct ptentry *ptn;

	ASSERT(ptable && ptable->count < MAX_PTABLE_PARTS);

	ptn = &ptable->parts[ptable->count++];
	strncpy(ptn->name, name, MAX_PTENTRY_NAME);
	ptn->start = start;
	ptn->length = length;
	ptn->flags = flags;
	ptn->type = type;
	ptn->perm = perm;
}

void ptable_dump(struct ptable *ptable)
{
	struct ptentry *ptn;
	int i;

	for (i = 0; i < ptable->count; ++i) {
		ptn = &ptable->parts[i];
		//Fix it lateron. Ptable dumping not so important for now -.-
		/*dprintf(INFO, "ptn %d name='%s' start=%08x len=%08x "
			"flags=%08x type=%s Writable=%s\n", i, ptn->name, ptn->start, ptn->length,
			ptn->flags, ptype[ptn->type], pperm[ptn->perm]);*/
		
		}
}

struct ptentry *ptable_find(struct ptable *ptable, const char *name)
{
	struct ptentry *ptn;
	int i;

	for (i = 0; i < ptable->count; ++i) {
		ptn = &ptable->parts[i];
		if (!strcmp(ptn->name, name))
			return ptn;
	}

	return NULL;
}

struct ptentry *ptable_get(struct ptable *ptable, int n)
{
	if (n >= ptable->count)
		return NULL;
	return &ptable->parts[n];
}

int ptable_size(struct ptable *ptable)
{
    return ptable->count;
}

/* koko : Count bad blocks in a partition */
int num_of_bad_blocks_in_part(const char *pName)
{
	int bad_blocks_num=0;
	for (int i = 0; i < block_tbl.count; i++)
	{
		if( (strlen(block_tbl.blocks[i].partition)!=0) && (!memcmp(block_tbl.blocks[i].partition, pName, strlen(pName))) )
			bad_blocks_num++;
	}

	return bad_blocks_num;
}

/* koko : Check if a partition which has bad blocks and size <=5MB exists */
int small_part_with_bad_blocks_exists()
{
	for (int i = 0; i < block_tbl.count; i++)
	{
		if( (strlen(block_tbl.blocks[i].partition)!=0) && (memcmp(block_tbl.blocks[i].partition, "ExtROM", strlen(block_tbl.blocks[0].partition))) && ((block_tbl.blocks[i].pos_from_pstart + block_tbl.blocks[i].pos_from_pend)<=40) )
			return 1;
	}

	return 0;
}