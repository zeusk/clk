/*
 * author: cedesmith
 * license: GPL
 */

#include <string.h>
#include <debug.h>
#include <dev/keys.h>
#include <dev/gpio_keypad.h>
#include <lib/ptable.h>
#include <dev/flash.h>
#include <smem.h>
#include <platform/iomap.h>
#include <reg.h>

#include "version.h"

#define LINUX_MACHTYPE  2524
#define HTCLEO_FLASH_OFFSET	0x219

static struct ptable flash_ptable;

// align data on a 512 boundary so will not be interrupted in nbh
static struct ptentry board_part_list[MAX_PTABLE_PARTS] __attribute__ ((aligned (512))) = {
		{
				.name = "PTABLE-MB", // PTABLE-BLK or PTABLE-MB for length in MB or BLOCKS
		},
		{
				.name = "misc",
				.length = 1 /* In MB */,
		},
		{
				.name = "recovery",
				.length = 5 /* In MB */,
		},
		{
				.name = "boot",
				.length = 5 /* In MB */,
		},
		{
				.name = "system",
				.length = SYSTEM_PARTITION_SIZE /* In MB */,
		},
		{
				.length = 44 /* In MB */,
				.name = "cache",
		},
		{
				.name = "userdata",
		},
};



static unsigned num_parts = sizeof(board_part_list)/sizeof(struct ptentry);
//#define part_empty(p) (p->name[0]==0 && p->start==0 && p->length==0 && p->flags==0 && p->type==0 && p->perm==0)
#define IS_PART_EMPTY(p) (p->name[0]==0)

extern unsigned load_address;
extern unsigned boot_into_recovery;

void keypad_init(void);
void display_init(void);
void display_lk_version();
void htcleo_ptable_dump(struct ptable *ptable);
void cmd_dmesg(const char *arg, void *data, unsigned sz);
void reboot(unsigned reboot_reason);
void target_display_init();
unsigned get_boot_reason(void);
void cmd_oem_register();
void target_init(void)
{
	struct flash_info *flash_info;
	unsigned start_block;
	unsigned blocks_per_plen = 1; //blocks per partition length
	unsigned nand_num_blocks;

	keys_init();
	keypad_init();

	uint16_t keys[] = {KEY_VOLUMEUP, KEY_VOLUMEDOWN, KEY_SOFT1, KEY_SEND, KEY_CLEAR, KEY_BACK, KEY_HOME};
	for(unsigned i=0; i< sizeof(keys)/sizeof(uint16_t); i++)
	if (keys_get_state(keys[i]) != 0)
	{
		display_init();
		display_lk_version();
		//dprintf(ALWAYS,"key %d pressed\n", i);
		break;
	}
	dprintf(INFO, "htcleo_init\n");

	if(get_boot_reason()==2) // booting for offmode charging, start recovery so kernel will charge phone
	{
		boot_into_recovery = 1;
		//dprintf(INFO, "reboot needed... \n");
		//reboot(0);
	}
	dprintf(ALWAYS, "load address %x\n", load_address);

	dprintf(INFO, "flash init\n");
	flash_init();
	flash_info = flash_get_info();
	ASSERT(flash_info);
	ASSERT(flash_info->num_blocks);
	nand_num_blocks = flash_info->num_blocks;

	ptable_init(&flash_ptable);

	if( strcmp(board_part_list[0].name,"PTABLE-BLK")==0 ) blocks_per_plen =1 ;
	else if( strcmp(board_part_list[0].name,"PTABLE-MB")==0 ) blocks_per_plen = (1024*1024)/flash_info->block_size;
	else panic("Invalid partition table\n");

	start_block = HTCLEO_FLASH_OFFSET;
	for (unsigned i = 1; i < num_parts; i++)
	{
		struct ptentry *ptn = &board_part_list[i];
		if( IS_PART_EMPTY(ptn) ) break;
		int len = ((ptn->length) * blocks_per_plen);

		if( ptn->start == 0 ) ptn->start = start_block;
		else if( ptn->start < start_block) panic("Partition %s start %x < %x\n", ptn->name, ptn->start, start_block);

		if(ptn->length == 0)
		{
			unsigned length_for_prt = 0;
			if( i<num_parts && !IS_PART_EMPTY((&board_part_list[i+1])) && board_part_list[i+1].start!=0)
			{
				length_for_prt =  board_part_list[i+1].start - ptn->start;
			}
			else
			{
				for (unsigned j = i+1; j < num_parts; j++)
				{
						struct ptentry *temp_ptn = &board_part_list[j];
						if( IS_PART_EMPTY(temp_ptn) ) break;
						if( temp_ptn->length==0 ) panic("partition %s and %s have variable length\n", ptn->name, temp_ptn->name);
						length_for_prt += ((temp_ptn->length) * blocks_per_plen);
				}
			}
			len = (nand_num_blocks - 1 - 186 - 4) - (ptn->start + length_for_prt);
			ASSERT(len >= 0);
		}

		start_block = ptn->start + len;
		ptable_add(&flash_ptable, ptn->name, ptn->start, len, ptn->flags, TYPE_APPS_PARTITION, PERM_WRITEABLE);
	}

	htcleo_ptable_dump(&flash_ptable);
	flash_set_ptable(&flash_ptable);
}
void display_lk_version()
{
	char *version = "cedesmith's LK (CLK) v";
	strcat(version,cLK_version);
	strcat(version,"\n");
	_dputs(version);
}
struct fbcon_config* fbcon_display(void);
void htcleo_fastboot_init()
{
	// off charge and recovery boot failed, reboot to normal mode
	if(get_boot_reason()==2) reboot(0);

	// display not initialized
	if(fbcon_display()==NULL)
	{
		display_init();
		display_lk_version();
		htcleo_ptable_dump(&flash_ptable);
	}

	cmd_oem_register();
}
void target_early_init(void)
{
	//cedesmith: write reset vector while we can as MPU kicks in after flash_init();
	writel(0xe3a00546, 0); //mov r0, #0x11800000
	writel(0xe590f004, 4); //ldr	r15, [r0, #4]
}
unsigned board_machtype(void)
{
    return LINUX_MACHTYPE;
}

void reboot_device(unsigned reboot_reason)
{
	writel(reboot_reason, 0x2FFB0000);
	writel(reboot_reason^0x004b4c63, 0x2FFB0004); //XOR with cLK signature
    reboot(reboot_reason);
}

unsigned boot_reason = 0xFFFFFFFF;
unsigned android_reboot_reason = 0;
unsigned check_reboot_mode(void);
unsigned get_boot_reason(void)
{
	if(boot_reason==0xFFFFFFFF)
	{
		boot_reason = readl(MSM_SHARED_BASE+0xef244);
		dprintf(INFO, "boot reason %x\n", boot_reason);
		if(boot_reason!=2)
		{
			if(readl(0x2FFB0000)==(readl(0x2FFB0004)^0x004b4c63))
			{
				android_reboot_reason = readl(0x2FFB0000);
				dprintf(INFO, "android reboot reason %x\n", android_reboot_reason);
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
    if (get_boot_reason() == 2) return 1;
    return 0;
}

int target_is_emmc_boot(void)
{
	return 0;
}

void htcleo_ptable_dump(struct ptable *ptable)
{
	struct ptentry *ptn;
	int i;

	for (i = 0; i < ptable->count; ++i) {
		ptn = &ptable->parts[i];
		dprintf(INFO, "ptn %d name='%s' start=%08x len=%08x end=%08x \n",i, ptn->name, ptn->start, ptn->length,  ptn->start+ptn->length);
	}
}

//cedesmith: current version of qsd8k platform missing display_shutdown so add it
void lcdc_shutdown(void);
void display_shutdown(void)
{
    lcdc_shutdown();
}
