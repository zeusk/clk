/*
	Variable Partition table definition
	vptable is virtual configuration partition with settings and partition layout configuration
    Copyright (c) 2011 - Danijel Posilovic - dan1j3l
*/

#define HTCLEO_ROM_OFFSET	0x212

#define PTN_ROMHDR		"romhdr"
#define PTN_DEV_INF		"devinf"
#define TAG_INFO		"DEVINFO"
#define PTN_TASK29		"task29"

#define DEV_PART_NAME_SZ	32
#define MAX_NUM_PART		12

extern struct ptable* flash_get_devinfo(void);

struct _part
{
	char name[DEV_PART_NAME_SZ];	// partition name (identifier in mtd device)
	short size;			// size in blocks 1Mb = 8 Blocks
	bool asize;			// auto-size and use all available space 1=yes 0=no
};
struct dev_info
{
	char tag[7];
	struct _part partition[MAX_NUM_PART];
	short extrom_enabled;
	short fill_bbt_at_start;
	short inverted_colors;
	short show_startup_info;
	short usb_detect;
	short cpu_freq;
	short boot_sel;
	short multi_boot_screen;
	short panel_brightness;
	short udc;
	short use_inbuilt_charging;
	short chg_threshold;
}device_info;

#define DECLARE_DEV_INFO_PTR     register volatile struct dev_info *di __asm ("r8")

/* koko : Added struct needed for rearrange partitions */
struct mirror_part
{
	char tag[7];
	char name[DEV_PART_NAME_SZ];
	short size;
	bool asize;
	short order;
};
struct mirror_dev_info
{
	char tag[7];
	struct mirror_part partition[MAX_NUM_PART];
	short extrom_enabled;
	short fill_bbt_at_start;
	short inverted_colors;
	short show_startup_info;
	short usb_detect;
	short cpu_freq;
	short boot_sel;
	short multi_boot_screen;
	short panel_brightness;
	short udc;
	short use_inbuilt_charging;
	short chg_threshold;
}mirror_info;

unsigned get_blk_per_mb();
unsigned get_flash_offset();
unsigned get_full_flash_size();
unsigned get_usable_flash_size();
unsigned get_ext_rom_offset();
unsigned get_ext_rom_size();

int read_device_info(struct dev_info *output);
int write_device_info(const struct dev_info *input);

int lk_size;

unsigned device_available_size();
int device_variable_exist();
bool device_partition_exist(const char* pName);
int device_partition_size(const char* pName);
int device_partition_order(const char* pName);
int mirror_partition_order(const char* pName);
unsigned device_boot_ptn_num();
void device_resize_asize();
short device_variable_size();
char *device_variable_name();
void device_add(const char *pData);
void device_add_ex(const char *pName, unsigned size, bool SizeGivenInBlocks);
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
void device_resize_ex(const char *pName, unsigned size, bool SizeGivenInBlocks);
