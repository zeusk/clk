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

extern struct ptable* flash_get_vptable(void);

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
	short size_fixed; // due to bad blocks
	short inverted_colors;
	short show_startup_info;
}device_info;

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
	short size_fixed; // due to bad blocks
	short inverted_colors;
	short show_startup_info;
}mirror_info;

unsigned get_blk_per_mb();
unsigned get_flash_offset();
unsigned get_full_flash_size();
unsigned get_usable_flash_size();
unsigned get_ext_rom_offset();
unsigned get_ext_rom_size();

int read_device_info(struct dev_info *output);
int write_device_info(const struct dev_info *input);

unsigned device_available_size();
int device_variable_exist();
bool device_partition_exist(const char* pName);
int device_partition_size(const char* pName);
int device_partition_order(const char* pName);
int mirror_partition_order(const char* pName);
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
