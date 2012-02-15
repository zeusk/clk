/*
 * author: cedesmith
 * license: GPL
 * Added inhouse partitioning code: dan1j3l
 * Optimized by zeus; inspired, consulted by cotulla, my bitch :D
 */

#include <string.h>
#include <debug.h>
#include <dev/keys.h>
#include <dev/gpio_keypad.h>
#include <kernel/thread.h>
#include <lib/ptable.h>
#include <lib/devinfo.h>
#include <dev/fbcon.h>
#include <dev/flash.h>
#include <smem.h>
#include <platform/iomap.h>
#include <reg.h>

#define LINUX_MACHTYPE  2524
#define FASTBOOT_MODE   0x77665500

static struct ptable flash_ptable;
static struct ptable flash_devinfo;

extern unsigned boot_into_recovery;
extern unsigned blocks_per_mb;
extern unsigned flash_start_blk;
extern unsigned flash_size_blk;

void keypad_init(void);
void display_init(void);
void htcleo_ptable_dump(struct ptable *ptable);
void cmd_dmesg(const char *arg, void *data, unsigned sz);
void reboot(unsigned reboot_reason);
void target_display_init();
void cmd_oem_register();
void shutdown_device(void);
void dispmid(const char *fmt, int sel);
void flash_set_vptable(struct ptable * new_ptable);

void reboot_device(unsigned reboot_reason);
unsigned get_boot_reason(void);
unsigned boot_reason = 0xFFFFFFFF;
unsigned android_reboot_reason = 0;

static char	buf[4096]; //Equal to max-supported page_size

struct PartEntry
{
	uint8_t BootInd;
	uint8_t FirstHead;
	uint8_t FirstSector;
	uint8_t FirstTrack;
	uint8_t FileSystem;
	uint8_t LastHead;
	uint8_t LastSector;
	uint8_t LastTrack;
	uint32_t StartSector;
	uint32_t TotalSectors;
};

int find_start_block()
{
	unsigned page_size = flash_page_size();

	// Get ROMHDR partition
	struct ptentry *ptn = ptable_find( &flash_devinfo, PTN_ROMHDR );
	if ( ptn == NULL )
	{
		dprintf( CRITICAL, "ERROR: ROMHDR partition not found!!!" );
		return -1;
	}

	// Read ROM header
	if ( flash_read( ptn, 0, buf, page_size ) )
	{
		dprintf( CRITICAL, "ERROR: can't read ROM header!!!\n" );
		return -1;
	}

	// Validate ROM signature
	if ( buf[0] != 0xE9 || buf[1] != 0xFD || buf[2] != 0xFF || buf[512-2] != 0x55 || buf[512-1] != 0xAA )
	{
		dprintf( CRITICAL, "ERROR: invalid ROM signature!!!\n" );
		return -1;
	}

	const char* part_buf = buf + 0x1BE;

	// Validate first partition layout
	struct PartEntry romptr;
	memcpy( &romptr, part_buf, sizeof( struct PartEntry ) );
	if ( romptr.BootInd != 0 || romptr.FileSystem != 0x23 || romptr.StartSector != 0x2 )
	{
		dprintf( CRITICAL, "ERROR: invalid ROM boot partition!!!\n" );
		return -1;
	}

	// Set flash start block (block where ROM boot partition ends)
	flash_start_blk = ptn->start + ( romptr.StartSector + romptr.TotalSectors ) / 64;

	// Validate second partition layout (must be empty)
	part_buf += sizeof( struct PartEntry );
	memcpy( &romptr, part_buf, sizeof( struct PartEntry ) );
	if ( romptr.FileSystem != 0 || romptr.StartSector != 0 || romptr.TotalSectors != 0 )
	{
		dprintf( CRITICAL, "ERROR: second partition detected!!!\n" );
		return -1;
	}

	// Read second sector
	if ( flash_read( ptn, page_size, buf, page_size ) )
	{
		dprintf( CRITICAL, "ERROR: can't read ROM header!!!\n" );
		return -1;
	}

	// Validate MSFLASH50
	if ( memcmp( buf, "MSFLSH50", sizeof( "MSFLSH50" ) - 1 ) )
	{
		dprintf( CRITICAL, "ERROR: can't find MSFLASH50 tag!!!\n" );
		return -1;
	}

	return 0;
}

void target_init(void)
{
	struct flash_info *flash_info;
	unsigned nand_num_blocks;
	
	keys_init();
	keypad_init();

	if ( get_boot_reason() == 2 ) 
		boot_into_recovery = 1;

	flash_init();
	flash_info = flash_get_info();

	ASSERT( flash_info );
	ASSERT( flash_info->num_blocks );

	nand_num_blocks = flash_info->num_blocks;
	blocks_per_mb	= ( 1024 * 1024 ) / flash_info->block_size;

	//////////////////////////////////////////////////////////////////////////
	// DEVINFO

	ptable_init( &flash_devinfo );

	// ROM header partition (read only)
	ptable_add( &flash_devinfo, PTN_ROMHDR, HTCLEO_ROM_OFFSET, 0x2, 0, TYPE_APPS_PARTITION, PERM_NON_WRITEABLE );

	// Find free ROM start block
	if ( find_start_block() == -1 )
	{
		// Display error
		display_init();
		dprintf( CRITICAL, "Failed to find free ROM start block, rebooting!!!\n" );
		thread_sleep( 20000 );

		// Invalid ROM partition, reboot device
		reboot_device( FASTBOOT_MODE );

		return;
	}

	// DEVINFO partition (1block = 128KBytes)
	ptable_add( &flash_devinfo, PTN_DEV_INF, flash_start_blk++, 1, 0, TYPE_APPS_PARTITION, PERM_WRITEABLE );

	// Calculate remaining flash size
	flash_size_blk = nand_num_blocks - flash_start_blk;

	// task29 partition (to format all partitions at once)
	ptable_add( &flash_devinfo, PTN_TASK29, flash_start_blk, flash_size_blk, 0, TYPE_APPS_PARTITION, PERM_WRITEABLE );

	flash_set_vptable( &flash_devinfo );

	init_device();

	//////////////////////////////////////////////////////////////////////////
	// PTABLE

	unsigned start_blk = flash_start_blk;

	ptable_init( &flash_ptable );

	// Parse partitions
	for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
	{
		// Valid partition?
		if ( strlen( device_info.partition[i].name ) == 0 )
			break;

		// Add partition
		ptable_add( &flash_ptable, device_info.partition[i].name, start_blk, device_info.partition[i].size, 0, TYPE_APPS_PARTITION, PERM_WRITEABLE );
		
		// Skip to next free block
		start_blk += device_info.partition[i].size;
	}
	
	flash_set_ptable( &flash_ptable );
}

/* koko : No reboot needed for changes to take effect if we use this */
static struct ptable flash_newptable;
void ptable_re_init(void)
{
	unsigned start_blk = flash_start_blk;
	ptable_init( &flash_ptable );
	ptable_init( &flash_newptable );

	for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
	{
		if ( strlen( device_info.partition[i].name ) == 0 )
			break;
		ptable_add( &flash_newptable, device_info.partition[i].name, start_blk, device_info.partition[i].size, 0, TYPE_APPS_PARTITION, PERM_WRITEABLE );
		start_blk += device_info.partition[i].size;
	}
	flash_ptable = flash_newptable;
}

struct fbcon_config* fbcon_display(void);

int htcleo_fastboot_init()
{
	cmd_oem_register();
	return 1;
}

void target_early_init(void)
{
	//cedesmith: write reset vector while we can as MPU kicks in after flash_init();
	writel(0xe3a00546, 0); //mov r0, #0x11800000
	writel(0xe590f004, 4); //ldr	r15, [r0, #4]
}

unsigned target_support_flashlight(void)
{
	return 1;
}

unsigned board_machtype(void)
{
    return LINUX_MACHTYPE;
}

void reboot_device(unsigned reboot_reason)
{
	fill_screen(0x0000);
	writel(reboot_reason, 0x2FFB0000);
	writel(reboot_reason^0x004b4c63, 0x2FFB0004); //XOR with cLK signature
    	reboot(reboot_reason);
}

unsigned get_boot_reason(void)
{
	if(boot_reason==0xFFFFFFFF)
	{
		boot_reason = readl(MSM_SHARED_BASE+0xef244);
		//dprintf(INFO, "boot reason %x\n", boot_reason);
		if(boot_reason!=2)
		{
			if(readl(0x2FFB0000)==(readl(0x2FFB0004)^0x004b4c63))
			{
				android_reboot_reason = readl(0x2FFB0000);
				//dprintf(INFO, "android reboot reason %x\n", android_reboot_reason);
				writel(0, 0x2FFB0000);
			}
		}
	}

	return boot_reason;
}

unsigned check_reboot_mode(void)
{
	get_boot_reason();
	return android_reboot_reason;
}

unsigned target_pause_for_battery_charge(void)
{
    if (get_boot_reason() == 2) 
		return 1;

    return 0;
}

int target_is_emmc_boot(void)
{
	return 0;
}

void htcleo_ptable_dump(struct ptable *ptable)
{
	ptable_dump(ptable);
}

//cedesmith: current version of qsd8k platform missing display_shutdown so add it
void lcdc_shutdown(void);
void display_shutdown(void)
{
    lcdc_shutdown();
}
