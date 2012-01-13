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

#define LIB_DEV_INF 1

#define DEV_MAGIC_1		"zeusk"
#define DEV_MAGIC_1_SZ		4

#define DEV_MAGIC_2		"rick"
#define DEV_MAGIC_2_SZ		4

#define DEV_PART_NAME_SZ	16
#define DEV_NUM_OF_PARTS	12

#define PTN_ROMHDR			"romhdr"
#define PTN_DEV_INF			"devinf"
#define PTN_TASK_29                       "task29"

#define EXT_ROM_BLOCKS 191
#define HTCLEO_ROM_OFFSET	0x212

struct dev_part
{
	char name[DEV_PART_NAME_SZ];	/* 16 byte */
	short size;	                                                        /* 02 byte */ 
	short asize;                                                        /* 02 byte */
};

struct device_info_str /* 254 byte */
{
	char ptable_magic_1[DEV_MAGIC_1_SZ];                      /* 004 bytes */
	struct dev_part partition[DEV_NUM_OF_PARTS];    /* 240 bytes */
	short extrom_enabled;                                                             /* 002 bytes */
	char ptable_magic_2[DEV_MAGIC_2_SZ];                     /* 004 bytes */
                                                                                                                        /* 250 bytes */
};

struct device_info_str device_info;

unsigned get_blk_per_mb();
unsigned get_flash_offset();
unsigned get_full_flash_size();
unsigned get_usable_flash_size();
unsigned get_ext_rom_offset();
unsigned get_ext_rom_size();

extern struct ptable* flash_get_vptable(void);

int read_device_info_nand(struct device_info_str *out);
int write_device_info_nand(const struct device_info_str *in);

unsigned device_available_size();
int device_variable_exist();
short device_partition_exist(const char* pName);
int device_partition_size(const char* pName);
void device_resize_asize();
short device_variable_size();
void device_add(const char *pData);
void device_add_ex(const char *pName, unsigned size);
void device_restruct();
void device_del(const char *pName);
void device_list();
void device_clear();
void device_create_default();
void init_device();
void device_commit();
void device_read();
void device_enable_extrom();
void device_disable_extrom();
void device_resize(const char *pData);
void device_resize_ex(const char *pName, unsigned size);
