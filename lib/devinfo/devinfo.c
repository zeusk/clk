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

#include <lib/devinfo.h>

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

int read_device_info(struct dev_info *out)
{	
	struct ptable* ptable = flash_get_devinfo();
	if ( ptable == NULL ) 
	{
		printf( "   ERROR: DEVINFO not found!!!\n" );
		return -1;
	}

	struct ptentry* ptn = ptable_find( ptable, PTN_DEV_INF );
	if ( ptn == NULL )
	{
		printf( "   ERROR: No DEVINFO partition!!!\n" );
		return -1;
	}

	if ( flash_read( ptn, 0, buf, sizeof( *out ) ) )
	{
		printf( "   ERROR: Cannot read DEVINFO header\n");
		return -1;
	}

	memcpy( out, buf, sizeof( *out ) );

	return 0;
}

int write_device_info(const struct dev_info *in)
{
	struct ptable* ptable = flash_get_devinfo();
	if ( ptable == NULL )
	{
		printf( "   ERROR: DEVINFO not found!!!\n" );
		return -1;
	}

	struct ptentry* ptn = ptable_find( ptable, PTN_DEV_INF );
	if ( ptn == NULL )
	{
		printf( "   ERROR: No DEVINFO partition!!!\n" );
		return -1;
	}

	unsigned pagesize = flash_page_size();
	if ( pagesize == 0 )
	{
		printf( "   ERROR: Invalid pagesize!!!\n" );
		return -1;
	}

	if ( sizeof( *in ) > pagesize )
	{
		printf( "   ERROR: Invalid DEVINFO header size!!!\n" );
		return -1;
	}

	memset( (void*) SCRATCH_ADDR, 0, pagesize );
	memcpy( (void*) SCRATCH_ADDR, (void*) in, sizeof( *in ) );

	if ( flash_write( ptn, 0, (void*) SCRATCH_ADDR, pagesize ) )
	{
		printf( "   ERROR: failed to write DEVINFO header!!!\n" );
		return -1;
	}

	return 0;
}

unsigned device_available_size()
{
	unsigned used = 0;

	for ( unsigned i = 1; i < MAX_NUM_PART; i++ ) //KoKo: Skip 1st partition - it is lk's partition - or else we'll leave 4 blocks unexploited
	{
		if ( strlen( device_info.partition[i].name ) != 0 && device_info.partition[i].asize == 0 )
			used += device_info.partition[i].size;
	}

	return get_usable_flash_size() - used;
}

bool device_variable_exist()
{
	for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
	{
		if ( strlen( device_info.partition[i].name ) != 0 && device_info.partition[i].asize == 1 ) 
			return 1;
	}

	return 0;
}

bool device_partition_exist(const char* pName)
{
	for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
	{
		if ( !memcmp( device_info.partition[i].name, pName, strlen( pName ) ) )
			return 1;
	}

	return 0;
}

int device_partition_size(const char* pName)
{
	for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
	{
		if ( !memcmp( device_info.partition[i].name, pName, strlen( pName ) ) )
			return device_info.partition[i].size;
	}

	return 0;
}

int device_partition_order(const char* pName)
{
	for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
	{
		if ( !memcmp( device_info.partition[i].name, pName, strlen( pName ) ) )
			return i+1;
	}

	return 0;
}

int mirror_partition_order(const char* pName)
{
	for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
	{
		if ( !memcmp( mirror_info.partition[i].name, pName, strlen( pName ) ) )
			return mirror_info.partition[i].order;
	}

	return 0;
}

void device_resize_asize()
{
	for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
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
	for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
	{
		if ( strlen( device_info.partition[i].name ) != 0 && device_info.partition[i].asize == 1 )
			return device_info.partition[i].size;
	}

	return -1;	// variable partition doesn't exist
}

void device_add(const char* pData)
{
	char buff[64];
	char name[32];
	char* tmp_buff;
	unsigned size;
	bool SizeGivenInBlocks;

	strcpy( buff, pData );

	tmp_buff = strtok( buff, ":" );
	strcpy( name, tmp_buff );
	tmp_buff = strtok( NULL, ":" );
	size = atoi( tmp_buff );

	/*
	 * KoKo: If the size is given in blocks
	 *	 the user must have entered a second ':' in the command
	 * 	 So we use that to detect if size is given in blocks or MB
	 */
	if (strtok( NULL, ":" ) == NULL)
	{
		SizeGivenInBlocks = false;
	}else{
		SizeGivenInBlocks = true;
	}

	device_add_ex( name, size, SizeGivenInBlocks );
}

void device_add_ex(const char *pName, unsigned size, bool SizeGivenInBlocks)
{
	if ( strlen( pName ) == 0 )
		return;

	if(!SizeGivenInBlocks)
	{
	// Convert from MB to blocks
	size = size * blocks_per_mb;
	}

	// A partition with same name exists?
	if ( device_partition_exist( pName ) )
		return;

	// Variable partition already exist?
	if ( size == 0 && device_variable_exist() )
		return;

	// Validate new partition size
	unsigned available_size = device_available_size();
	unsigned required_size	= ( device_variable_exist() ? blocks_per_mb : 0 ) + ( size ? size : blocks_per_mb ); // min 1 MB for variable partition
	if ( available_size < required_size )
		return;

	for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
	{
		// Find first empty partition
		if ( strlen( device_info.partition[i].name ) == 0 )
		{
			strcpy( device_info.partition[i].name, pName );

			if ( size == 0 )
			{
				// Variable partition, set available size
				device_info.partition[i].size	= available_size;
				device_info.partition[i].asize	= 1;
			}
			else
			{
				// Fixed partition, recalculate variable partition
				device_info.partition[i].size	= size;
				device_info.partition[i].asize	= 0;
				device_resize_asize();
			}
			
			break;
		}
	}
}

void device_resize(const char *pData)
{
	char buff[64];
	char name[32];
	char* tmp_buff;
	unsigned size;
	bool SizeGivenInBlocks;

	strcpy( buff, pData );

	tmp_buff = strtok( buff, ":" );
	strcpy( name, tmp_buff );
	tmp_buff = strtok( NULL, ":" );
	size = atoi( tmp_buff );

	/*
	 * KoKo: If the size is given in blocks
	 *	 the user must have entered a second ':' in the command
	 * 	 So we use that to detect if size is given in blocks or MB
	 */
	if (strtok( NULL, ":" ) == NULL)
	{
		SizeGivenInBlocks = false;
	}else{
		SizeGivenInBlocks = true;
	}

	device_resize_ex( name, size, SizeGivenInBlocks );
}

void device_resize_ex(const char *pName, unsigned size, bool SizeGivenInBlocks)
{
	if ( strlen( pName ) == 0 )
			return;
	
	if ( memcmp( pName, "lk", strlen( pName ) ) )
	{
		if(!SizeGivenInBlocks)
		{
		// Convert from MB to blocks
		size = size * blocks_per_mb;
		}
	
		// Find partition
		unsigned i;
		for ( i = 0; i < MAX_NUM_PART; i++ )
		{
			if ( !memcmp( device_info.partition[i].name, pName, strlen( pName ) ) )
				break;
		}
	
		// Partition exist?
		if ( i >= MAX_NUM_PART )
			return;
	
		// Variable partition?
		if ( size == 0 )
		{
			// Already variable partition
			if ( device_info.partition[i].asize )
			{
				// Update size just to be sure
				device_info.partition[i].size = device_available_size();
				return;
			}
			
			// Another partition is set as variable
			if ( device_variable_exist() )
				return;
		}
	
		// Validate new partition size
		unsigned available_size = device_available_size() + ( device_info.partition[i].asize ? 0 : device_info.partition[i].size );
		unsigned required_size  = ( device_variable_exist() ? blocks_per_mb : 0 ) + ( size ? size : blocks_per_mb );	// 1MB reserved for variable partition
		if ( available_size < required_size )
			return;
	
		if ( size == 0 )
		{
			// Variable partition, set available size
			device_info.partition[i].size	= available_size;
			device_info.partition[i].asize	= 1;
		}
		else
		{
			// Fixed partition, recalculate variable partition
			device_info.partition[i].size	= size;
			device_info.partition[i].asize	= 0;
			device_resize_asize();
		}
	}
}

// Restruct ptable and remove empty space between partitions
void device_restruct()
{
	unsigned next = 0;
	for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
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
				device_info.partition[i].size	= 0;
				device_info.partition[i].asize	= 0;
			}

			next++;
		}
	}
}

// Remove partition from ptable
void device_del(const char *pName)
{
	if ( memcmp( pName, "lk", strlen( pName ) ) ) //KoKo: DO NOT allow 'lk' partition to be deleted
	{
		int removed = 0;
	
		for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
		{
			if ( !memcmp( device_info.partition[i].name, pName, strlen( pName ) ) )
			{
				strcpy( device_info.partition[i].name, "" );
				device_info.partition[i].size	= 0;
				device_info.partition[i].asize	= 0;
	
				removed++;
			}
		}
	
		if ( removed )
		{
			device_restruct();
			device_resize_asize();
		}
	}
}

// clear current layout
void device_clear()
{
	strcpy( device_info.tag, "" );
	for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
	{
			strcpy( device_info.partition[i].name, "" );
			device_info.partition[i].size	= 0;
			device_info.partition[i].asize	= 0;
	}
}

// Print current ptable
#define round(x) ((long)((x)+0.5))
void device_list()
{
	printf("    ____________________________________________________ \n\
		   |                  PARTITION  TABLE                  |\n\
		   |____________________________________________________|\n\
		   | MTDBLOCK# |   NAME   | AUTO-SIZE |  BLOCKS  |  MB  |\n");
	printf("   |===========|==========|===========|==========|======|\n");
	for ( unsigned i = 1; i < MAX_NUM_PART; i++ )
	{
		if ( strlen( device_info.partition[i].name ) == 0 )
			break;
		
      		printf( "   | mtdblock%i | %8s |     %i     |   %4i   | %3i  |\n", i, device_info.partition[i].name,
      			device_info.partition[i].asize, device_info.partition[i].size, round((float)(device_info.partition[i].size) / (float)(get_blk_per_mb())) );
	}
	printf("   |___________|__________|___________|__________|______|\n");
}

// Create default partition layout
/* koko : Changed default ptable so that cache is last part */
void device_create_default()
{
	int lk_size = (device_info.partition[0].size == 0 ? 3 : device_info.partition[0].size);
	char lk_prt[64];
	sprintf( lk_prt, "lk:%d:b", lk_size );
	
	device_clear();
	
	device_add(lk_prt);
	device_add( "recovery:5" );
	device_add( "misc:1" );
	device_add( "boot:5" );
	device_add( "system:150" );
	device_add( "userdata:0" );
	if(device_info.extrom_enabled){
	device_add( "null:1:b" );
	device_add( "cache:191:b" );
	}else{
	device_add( "cache:5" );
	}
}

void device_commit()
{
	strcpy( device_info.tag, TAG_INFO );
	write_device_info( &device_info );
}

void device_read()
{
	device_clear();
	read_device_info( &device_info );
}

void device_enable_extrom()
{
	device_info.extrom_enabled = 1;
	device_resize_asize();
	device_commit();
}

void device_disable_extrom()
{
	device_info.extrom_enabled = 0;
	device_resize_asize();
	device_commit();
}

// Init and load ptable from NAND
void init_device()
{
	device_read();

	// Look for VPTABLE, if doesn't exist create default one
	if ( strcmp( device_info.tag, TAG_INFO ) )
	{
		device_info.extrom_enabled = 0;
		device_info.size_fixed = 0;
		device_info.inverted_colors = 0;
		device_info.show_startup_info = 0;
		device_info.usb_detect = 0;
		device_create_default();
		device_commit();
	}
	else
	{
		// Update variable partition size when required
		if ( device_variable_size() == 0 )
		{
			device_resize_asize();
			device_commit();
		}
	}
}
