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

#include <arch/arm.h>
#include <arch/ops.h>

#include <dev/flash.h>
#include <dev/keys.h>
#include <dev/udc.h>

#include <kernel/thread.h>

#include <lib/ptable.h>
#include <lib/devinfo.h>

#include <debug.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned blocks_per_mb = 1;
unsigned flash_start_blk = HTCLEO_ROM_OFFSET;
unsigned flash_size_blk = 0;

static char buf[4096];

unsigned get_blk_per_mb()
{
	return blocks_per_mb;
}

unsigned get_flash_offset()
{
	return flash_start_blk;
}

unsigned get_full_flash_size()
{
	return flash_size_blk;
}

unsigned get_usable_flash_size()
{
	return flash_size_blk - ( device_info.extrom_enabled ? 0 : EXT_ROM_BLOCKS );
}

unsigned get_ext_rom_offset()
{
	return flash_size_blk - EXT_ROM_BLOCKS;
}

unsigned get_ext_rom_size()
{
	return EXT_ROM_BLOCKS;
}

int read_device_info_nand(struct device_info_str *out)
{	
	struct ptable* ptable = flash_get_temp_ptable();
	if ( ptable == NULL ) 
	{
		dprintf( CRITICAL, "ERROR: DEV_INF_PTABLE not found!!!\n" );
		return -1;
	}
	struct ptentry* ptn = ptable_find( ptable, PTN_DEV_INF );
	if ( ptn == NULL )
	{
		dprintf( CRITICAL, "ERROR: DEV_INF_PTN not found!!!\n" );
		return -1;
	}
	if ( flash_read( ptn, 0, buf, sizeof( *out ) ) )
	{
		dprintf( CRITICAL, "ERROR: Cannot read DEV_INF_PTN header\n");
		return -1;
	}
	memcpy( out, buf, sizeof( *out ) );
	return 0;
}

int write_device_info_nand(const struct device_info_str *in)
{
	struct ptable* ptable = flash_get_temp_ptable();
	if ( ptable == NULL ) 
	{
		dprintf( CRITICAL, "ERROR: DEV_INF_PTABLE not found!!!\n" );
		return -1;
	}
	struct ptentry* ptn = ptable_find( ptable, PTN_DEV_INF );
	if ( ptn == NULL )
	{
		dprintf( CRITICAL, "ERROR: DEV_INF_PTN not found!!!\n" );
		return -1;
	}
	unsigned pagesize = flash_page_size();
	if ( pagesize == 0 )
	{
		printf( CRITICAL, "ERROR: Invalid pagesize!!!\n" );
		return -1;
	}

	if ( sizeof( *in ) > pagesize )
	{
		printf( CRITICAL, "ERROR: DEV_INF_PTN header too large!!!\n" );
		return -1;
	}
	memset( (void*) SCRATCH_ADDR, 0, pagesize );
	memcpy( (void*) SCRATCH_ADDR, (void*) in, sizeof( *in ) );
	if (flash_write( ptn, 0, (void*) SCRATCH_ADDR, pagesize ) )
	{
		printf( CRITICAL, "ERROR: failed to write VPTABLE header!!!\n" );
		return -1;
	}
	return 0;
}

unsigned device_available_size()
{
	unsigned used = 0;

	for ( unsigned i = 0; i < DEV_NUM_OF_PARTS; i++ )
	{
		if ( strlen( device_info.partition[i].name ) != 0 && device_info.partition[i].asize == 0 )
			used += device_info.partition[i].size;
	}
	return ((unsigned)(get_usable_flash_size() - used));
}

short device_variable_part_exist()
{
	for ( unsigned i = 0; i < DEV_NUM_OF_PARTS; i++ )
	{
		if ( strlen( device_info.partition[i].name ) != 0 && device_info.partition[i].asize == 1 ) 
			return 1;
	}
	return 0;
}

short device_partition_exist(const char* pName)
{
	for ( unsigned i = 0; i < DEV_NUM_OF_PARTS; i++ )
	{
		if ( !memcmp( device_info.partition[i].name, pName, strlen( pName ) ) )
			return 1;
	}

	return 0;
}

int device_partition_size(const char* pName)
{
	for ( unsigned i = 0; i < DEV_NUM_OF_PARTS; i++ )
	{
		if ( !memcmp( device_info.partition[i].name, pName, strlen( pName ) ) )
			return device_info.partition[i].size;
	}
	return 0;
}

void device_resize_asize_part()
{
	for ( unsigned i = 0; i < DEV_NUM_OF_PARTS; i++ )
	{
		if ( strlen( device_info.partition[i].name ) != 0 && device_info.partition[i].asize == 1 )
		{
			device_info.partition[i].size = device_available_size();
			return;
		}
	}
}

short device_variable_size()
{
	for ( unsigned i = 0; i < DEV_NUM_OF_PARTS; i++ )
	{
		if ( strlen( device_info.partition[i].name ) != 0 && device_info.partition[i].asize == 1 )
			return device_info.partition[i].size;
	}
	return -1;
}
void device_part_add(const char* pData)
{
	char buff[64];
	char name[32];
	char* tmp_buff;
	unsigned size;

	strcpy( buff, pData );

	tmp_buff = strtok( buff, ":" );
	strcpy( name, tmp_buff );
	tmp_buff = strtok( NULL, ":" );
	size = atoi( tmp_buff );

	device_part_add_helper( name, size );
}

void device_part_add_helper(const char *pName, unsigned size)
{
	if ( strlen( pName ) == 0 )
		return;

	// Convert from MB to blocks
	size = size * blocks_per_mb;

	// A partition with same name exists?
	if ( device_partition_exist( pName ) )
		return;

	// Variable partition already exist?
	if ( size == 0 && device_variable_part_exist() )
		return;

	// Validate new partition size
	unsigned available_size = device_available_size();
	unsigned required_size	= (device_variable_part_exist() ? blocks_per_mb : 0 ) + ( size ? size : blocks_per_mb ); // min 1 MB for variable partition
	if ( available_size < required_size )
		return;

	for ( unsigned i = 0; i < DEV_NUM_OF_PARTS; i++ )
	{
		// Find first empty partition
		if ( strlen( device_info.partition[i].name ) == 0 )
		{
			strcpy( device_info.partition[i].name, pName );

			if ( size == 0 )
			{
				// Variable partition, set available size
				device_info.partition[i].size		= available_size;
				device_info.partition[i].asize	= 1;
			}
			else
			{
				// Fixed partition, recalculate variable partition
				device_info.partition[i].size		= size;
				device_info.partition[i].asize	= 0;
				device_resize_asize_part();
			}

			break;
		}
	}
}

void device_part_resize(const char *pData)
{
	char buff[64];
	char name[32];
	char* tmp_buff;
	unsigned size;

	strcpy( buff, pData );

	tmp_buff = strtok( buff, ":" );
	strcpy( name, tmp_buff );
	tmp_buff = strtok( NULL, ":" );
	size = atoi( tmp_buff );
	
	device_resize_helper( name, size );
}

void device_resize_helper(const char *pName, unsigned size)
{
	if ( strlen( pName ) == 0 )
		return;

	// Convert from MB to blocks
	size = size * blocks_per_mb;

	// Find partition
	unsigned i;
	for ( i = 0; i < DEV_NUM_OF_PARTS; i++ )
	{
		if ( !memcmp( device_info.partition[i].name, pName, strlen( pName ) ) )
			break;
	}

	// Partition exist?
	if ( i >= DEV_NUM_OF_PARTS )
		return;

	// Variable partition?
	if ( size == 0 )
	{
		// Already variable partition
		if ( device_info.partition[i].asize )
		{
			// Update size just to be sure
			device_info.partition[i].size = vpart_available_size();
			return;
		}
		
		// Another partition is set as variable
		if ( vpart_variable_exist() )
			return;
	}

	// Validate new partition size
	unsigned available_size = vpart_available_size() + ( device_info.partition[i].asize ? 0 : device_info.partition[i].size );
	unsigned required_size  = ( vpart_variable_exist() ? blocks_per_mb : 0 ) + ( size ? size : blocks_per_mb );	// 1MB reserved for variable partition
	if ( available_size < required_size )
		return;

	if ( size == 0 )
	{
		// Variable partition, set available size
		device_info.partition[i].size		= available_size;
		device_info.partition[i].asize	= 1;
	}
	else
	{
		// Fixed partition, recalculate variable partition
		device_info.partition[i].size		= size;
		device_info.partition[i].asize	= 0;
		device_resize_asize_part();
	}
}

void device_restruct_ptable()
{
	unsigned next = 0;
	for ( unsigned i = 0; i < DEV_NUM_OF_PARTS; i++ )
	{
		if ( strlen( device_info.partition[i].name ) != 0 )
		{
			// Need to move it?
			if ( next < i )
			{
				// Move partition
				strcpy( device_info.partition[next].name, device_info.partition[i].name );
				device_info.partition[next].size	= device_info.partition[i].size;
				device_info.partition[next].asize	= device_info.partition[i].asize;

				// Reset current
				strcpy( device_info.partition[i].name, "" );
				device_info.partition[i].size		= 0;
				device_info.partition[i].asize	= 0;
			}
			next++;
		}
	}
}

// Remove partition from ptable
void device_del(const char *pName)
{
	for ( unsigned i = 0; i < DEV_NUM_OF_PARTS; i++ )
	{
		if ( !memcmp( device_info.partition[i].name, pName, strlen( pName ) ) )
		{
			strcpy( device_info.partition[i].name, "" );
			device_info.partition[i].size		= 0;
			device_info.partition[i].asize	= 0;
			device_restruct_ptable();
			device_resize_asize_part();
		}
	}
	return;
}

// clear current layout
void device_info_clear()
{
	strcpy( device_info.ptable_magic_1, "" );
	strcpy( device_info.ptable_magic_2, "" );
	for ( unsigned i = 0; i < DEV_NUM_OF_PARTS; i++ )
	{
		strcpy( device_info.partition[i].name, "" );
		device_info.partition[i].size		= 0;
		device_info.partition[i].asize	= 0;
	}
}

// Print current ptable
void device_info_part_list()
{
	printf( "\n\n=============================== PARTITION TABLE ==============================\n\n" );

	for ( unsigned i = 0; i < DEV_NUM_OF_PARTS; i++ )
	{
		if ( strlen( device_info.partition[i].name ) == 0 )
			break;
		
		printf( "%i, name=%s, auto=%i, blocks=%i, size=%i MB\n", i+1, device_info.partition[i].name,
			device_info.partition[i].asize, device_info.partition[i].size, device_info.partition[i].size / get_blk_per_mb() );
	}

	printf( "\n\n=============================== PARTITION TABLE ==============================\n\n" );
}

// Create default partition layout
void device_info_default()
{
	device_info_clear();
	device_part_add( "misc:1" );
	device_part_add( "recovery:5" );
	device_part_add( "boot:5" );
	device_part_add( "system:150" );
	device_part_add( "cache:5" );
	device_part_add( "userdata:0" );
}

void device_info_commit()
{
	strcpy(device_info.ptable_magic_1, DEV_MAGIC_1);
	strcpy(device_info.ptable_magic_2, DEV_MAGIC_2);
	write_device_info_nand( &device_info );
}

void device_info_read()
{
	device_info_clear();
	read_device_info_nand( &device_info );
}

void device_enable_extrom()
{
	device_info.extrom_enabled = 1;
	device_resize_asize_part();
	device_info_commit();
}

void device_disable_extrom()
{
	device_info.extrom_enabled = 0;
	device_resize_asize_part();
	device_info_commit();
}

void init_device()
{
	device_info_read();
	if ((strcmp(device_info.ptable_magic_1, DEV_MAGIC_1))&&(strcmp(device_info.ptable_magic_2, DEV_MAGIC_2)))
	{
		device_info.extrom_enabled = 0;
		device_info_default();
		device_info_commit();
	}
	else
	{
		if ( device_variable_size() < 1) /* can be 0 due to commit failure or -1 due to sizing error */
		{
			device_resize_asize_part();
			device_info_commit();
		}
	}
}
