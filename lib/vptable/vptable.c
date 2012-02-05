/*
 Simple routines for reading and writing partition table
 Copyright (c) 2011 - Danijel Posilovic - dan1j3l
*/

#include <debug.h>
#include <arch/arm.h>
#include <dev/udc.h>
#include <string.h>
#include <kernel/thread.h>
#include <arch/ops.h>

#include <dev/flash.h>
#include <lib/ptable.h>
#include <dev/keys.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lib/vptable.h>

#define EXT_ROM_BLOCKS 191

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
	return flash_size_blk - ( vparts.extrom_enabled ? 0 : EXT_ROM_BLOCKS );
}

unsigned get_ext_rom_offset()
{
	return flash_size_blk - EXT_ROM_BLOCKS;
}

unsigned get_ext_rom_size()
{
	return EXT_ROM_BLOCKS;
}

int read_vptable(struct vpartitions *out)
{	
	struct ptable* ptable = flash_get_vptable();
	if ( ptable == NULL ) 
	{
		dprintf( CRITICAL, "   ERROR: VPTABLE not found!!!\n" );
		return -1;
	}

	struct ptentry* ptn = ptable_find( ptable, PTN_VPTABLE );
	if ( ptn == NULL )
	{
		dprintf( CRITICAL, "   ERROR: No VPTABLE partition!!!\n" );
		return -1;
	}

	if ( flash_read( ptn, 0, buf, sizeof( *out ) ) )
	{
		dprintf( CRITICAL, "   ERROR: Cannot read VPTABLE header\n");
		return -1;
	}

	memcpy( out, buf, sizeof( *out ) );

	return 0;
}

int write_vptable(const struct vpartitions *in)
{
	struct ptable* ptable = flash_get_vptable();
	if ( ptable == NULL )
	{
		printf( CRITICAL, "   ERROR: VPTABLE table not found!!!\n" );
		return -1;
	}

	struct ptentry* ptn = ptable_find( ptable, PTN_VPTABLE );
	if ( ptn == NULL )
	{
		printf( CRITICAL, "   ERROR: No VPTABLE partition!!!\n" );
		return -1;
	}

	unsigned pagesize = flash_page_size();
	if ( pagesize == 0 )
	{
		printf( CRITICAL, "   ERROR: Invalid pagesize!!!\n" );
		return -1;
	}

	if ( sizeof( *in ) > pagesize )
	{
		printf( CRITICAL, "   ERROR: Invalid VPTABLE header size!!!\n" );
		return -1;
	}

	memset( (void*) SCRATCH_ADDR, 0, pagesize );
	memcpy( (void*) SCRATCH_ADDR, (void*) in, sizeof( *in ) );

	if ( flash_write( ptn, 0, (void*) SCRATCH_ADDR, pagesize ) )
	{
		printf( CRITICAL, "   ERROR: failed to write VPTABLE header!!!\n" );
		return -1;
	}

	return 0;
}

unsigned vpart_available_size()
{
	unsigned used = 0;

	for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
	{
		if ( strlen( vparts.pdef[i].name ) != 0 && vparts.pdef[i].asize == 0 )
			used += vparts.pdef[i].size;
	}

	return get_usable_flash_size() - used;
}

bool vpart_variable_exist()
{
	for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
	{
		if ( strlen( vparts.pdef[i].name ) != 0 && vparts.pdef[i].asize == 1 ) 
			return 1;
	}

	return 0;
}

bool vpart_partition_exist(const char* pName)
{
	for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
	{
		if ( !memcmp( vparts.pdef[i].name, pName, strlen( pName ) ) )
			return 1;
	}

	return 0;
}

int vpart_partition_size(const char* pName)
{
	for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
	{
		if ( !memcmp( vparts.pdef[i].name, pName, strlen( pName ) ) )
			return vparts.pdef[i].size;
	}

	return 0;
}

int vpart_partition_order(const char* pName)
{
	for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
	{
		if ( !memcmp( vparts.pdef[i].name, pName, strlen( pName ) ) )
			return i+1;
	}

	return 0;
}

int mirrorpart_partition_order(const char* pName)
{
	for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
	{
		if ( !memcmp( mirrorparts.pdef[i].name, pName, strlen( pName ) ) )
			return mirrorparts.pdef[i].order;
	}

	return 0;
}

void vpart_resize_asize()
{
	for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
	{
		if ( strlen( vparts.pdef[i].name ) != 0 && vparts.pdef[i].asize == 1 )
		{
			vparts.pdef[i].size = vpart_available_size();
			return;
		}
	}
}

short vpart_variable_size()
{
	for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
	{
		if ( strlen( vparts.pdef[i].name ) != 0 && vparts.pdef[i].asize == 1 )
			return vparts.pdef[i].size;
	}

	return -1;	// variable partition doesn't exist
}

void vpart_add(const char* pData)
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

	vpart_add_ex( name, size );
}

void vpart_add_ex(const char *pName, unsigned size)
{
	if ( strlen( pName ) == 0 )
		return;

	// Convert from MB to blocks
	size = size * blocks_per_mb;

	// A partition with same name exists?
	if ( vpart_partition_exist( pName ) )
		return;

	// Variable partition already exist?
	if ( size == 0 && vpart_variable_exist() )
		return;

	// Validate new partition size
	unsigned available_size = vpart_available_size();
	unsigned required_size	= ( vpart_variable_exist() ? blocks_per_mb : 0 ) + ( size ? size : blocks_per_mb ); // min 1 MB for variable partition
	if ( available_size < required_size )
		return;

	for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
	{
		// Find first empty partition
		if ( strlen( vparts.pdef[i].name ) == 0 )
		{
			strcpy( vparts.pdef[i].name, pName );

			if ( size == 0 )
			{
				// Variable partition, set available size
				vparts.pdef[i].size	= available_size;
				vparts.pdef[i].asize	= 1;
			}
			else
			{
				// Fixed partition, recalculate variable partition
				vparts.pdef[i].size	= size;
				vparts.pdef[i].asize	= 0;
				vpart_resize_asize();
			}
			
			break;
		}
	}
}

void vpart_resize(const char *pData)
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
	
	vpart_resize_ex( name, size );
}

void vpart_resize_ex(const char *pName, unsigned size)
{
	if ( strlen( pName ) == 0 )
		return;

	// Convert from MB to blocks
	size = size * blocks_per_mb;

	// Find partition
	unsigned i;
	for ( i = 0; i < MAX_NUM_PART; i++ )
	{
		if ( !memcmp( vparts.pdef[i].name, pName, strlen( pName ) ) )
			break;
	}

	// Partition exist?
	if ( i >= MAX_NUM_PART )
		return;

	// Variable partition?
	if ( size == 0 )
	{
		// Already variable partition
		if ( vparts.pdef[i].asize )
		{
			// Update size just to be sure
			vparts.pdef[i].size = vpart_available_size();
			return;
		}
		
		// Another partition is set as variable
		if ( vpart_variable_exist() )
			return;
	}

	// Validate new partition size
	unsigned available_size = vpart_available_size() + ( vparts.pdef[i].asize ? 0 : vparts.pdef[i].size );
	unsigned required_size  = ( vpart_variable_exist() ? blocks_per_mb : 0 ) + ( size ? size : blocks_per_mb );	// 1MB reserved for variable partition
	if ( available_size < required_size )
		return;

	if ( size == 0 )
	{
		// Variable partition, set available size
		vparts.pdef[i].size		= available_size;
		vparts.pdef[i].asize	= 1;
	}
	else
	{
		// Fixed partition, recalculate variable partition
		vparts.pdef[i].size		= size;
		vparts.pdef[i].asize	= 0;
		vpart_resize_asize();
	}
}

// Restruct ptable and remove empty space between partitions
void vpart_restruct()
{
	unsigned next = 0;
	for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
	{
		if ( strlen( vparts.pdef[i].name ) != 0 )
		{
			// Need to move it?
			if ( next < i )
			{
				// Move partition
				strcpy( vparts.pdef[next].name, vparts.pdef[i].name );
				vparts.pdef[next].size	= vparts.pdef[i].size;
				vparts.pdef[next].asize	= vparts.pdef[i].asize;

				// Reset current
				strcpy( vparts.pdef[i].name, "" );
				vparts.pdef[i].size		= 0;
				vparts.pdef[i].asize	= 0;
			}

			next++;
		}
	}
}

// Remove partition from ptable
void vpart_del(const char *pName)
{
	int removed = 0;

	for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
	{
		if ( !memcmp( vparts.pdef[i].name, pName, strlen( pName ) ) )
		{
			strcpy( vparts.pdef[i].name, "" );
			vparts.pdef[i].size		= 0;
			vparts.pdef[i].asize	= 0;

			removed++;
		}
	}

	if ( removed )
	{
		vpart_restruct();
		vpart_resize_asize();
	}
}

// clear current layout
void vpart_clear()
{
	strcpy( vparts.tag, "" );
	for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
	{
		strcpy( vparts.pdef[i].name, "" );
		vparts.pdef[i].size		= 0;
		vparts.pdef[i].asize	= 0;
	}
}

// Print current ptable
void vpart_list()
{
	printf("    ____________________________________________________ \n\
		   |                  PARTITION  TABLE                  |\n\
		   |____________________________________________________|\n\
		   | MTDBLOCK# |   NAME   | AUTO-SIZE |  BLOCKS  |  MB  |\n");
	printf("   |===========|==========|===========|==========|======|\n");
	for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
	{
		if ( strlen( vparts.pdef[i].name ) == 0 )
			break;
		
		printf( "   | mtdblock%i | %8s |     %i     |   %4i   | %3i  |\n", i, vparts.pdef[i].name,
			vparts.pdef[i].asize, vparts.pdef[i].size, vparts.pdef[i].size / get_blk_per_mb() );
	}
	printf("   |___________|__________|___________|__________|______|\n");
}

// Create default partition layout
/* koko : Changed default ptable so that cache is last part */
void vpart_create_default()
{
	vpart_clear();
	vpart_add( "recovery:5" );
	vpart_add( "misc:1" );
	vpart_add( "boot:5" );
	vpart_add( "system:150" );
	vpart_add( "userdata:0" );
	vpart_add( "cache:5" );
}

void vpart_commit()
{
	strcpy( vparts.tag, TAG_VPTABLE );
	write_vptable( &vparts );
}

void vpart_read()
{
	vpart_clear();
	read_vptable( &vparts );
}

void vpart_enable_extrom()
{
	vparts.extrom_enabled = 1;
	vpart_resize_asize();
	vpart_commit();
}

void vpart_disable_extrom()
{
	vparts.extrom_enabled = 0;
	vpart_resize_asize();
	vpart_commit();
}

// Init and load ptable from NAND
void init_vpart()
{
	vpart_read();

	// Look for VPTABLE, if doesn't exist create default one
	if ( strcmp( vparts.tag, TAG_VPTABLE ) )
	{
		vparts.extrom_enabled = 0;
		vparts.size_fixed_due_to_bad_blocks = 0;
		vpart_create_default();
		vpart_commit();
	}
	else
	{
		// Update variable partition size when required
		if ( vpart_variable_size() == 0 )
		{
			vpart_resize_asize();
			vpart_commit();
		}
	}
}
