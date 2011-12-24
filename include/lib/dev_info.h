/*
 * Copyright (c) 2011, Shantanu/zeusk (shans95g@gmail.com)
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

#ifndef __LIB_DEV_INF_H
#define __LIB_DEV_INF_H

#define DEV_MAGIC_1		"shantanu"
#define DEV_MAGIC_1_SZ		8

#define DEV_MAGIC_2		"omfg"
#define DEV_MAGIC_2_SZ		4

#define DEV_PART_NAME_SZ	16
#define DEV_NUM_OF_PARTS	12

#define PTN_DEV_INF		"devinfo"

struct dev_part
{
	char name[DEV_PART_NAME_SZ];	/* 16 byte */
	unsigned length;		/* 4 byte */
};

struct device_inf /* 254 byte */
{
	char ptable_magic_1[DEV_MAGIC_1_SZ];		/* 008 bytes */
	struct dev_part partition[DEV_NUM_OF_PARTS];	/* 240 bytes */
	short extrom_enabled;				/* 002 bytes */
	char ptable_magic_2[DEV_MAGIC_2_SZ];		/* 004 bytes */
};

#endif /* __LIB_DEV_INF_H */
