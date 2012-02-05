/*
	Variable Partition table definition
	vptable is virtual configuration partition with settings and partition layout configuration
    Copyright (c) 2011 - Danijel Posilovic - dan1j3l
*/

//extern int atoi(const char *str);
extern struct ptable* flash_get_vptable(void);

struct part_def
{
	char name[32];	// partition name (identifier in mtd device)
	short size;		// size in blocks 1Mb = 8 Blocks
	bool asize;		// auto-size and use all available space 1=yes 0=no
};
struct vpartitions
{
	char tag[7];
	struct part_def pdef[12];
	short extrom_enabled;
	short size_fixed_due_to_bad_blocks;
};
struct vpartitions vparts;

/* koko : Added struct needed for rearrange partitions */
struct mirror_part_def
{
	char name[32];
	short size;
	bool asize;
	short order;
};
struct mirrorpartitions
{
	char tag[7];
	struct mirror_part_def pdef[12];
	short extrom_enabled;
	short size_fixed_due_to_bad_blocks;
};
struct mirrorpartitions mirrorparts;

static const unsigned MAX_NUM_PART = sizeof(vparts.pdef) / sizeof(vparts.pdef[0]);

#define HTCLEO_ROM_OFFSET	0x212

#define PTN_ROMHDR			"romhdr"
#define PTN_VPTABLE			"vptable"
#define PTN_TASK29			"task29"
#define TAG_VPTABLE			"VPTABLE"

unsigned get_blk_per_mb();
unsigned get_flash_offset();
unsigned get_full_flash_size();
unsigned get_usable_flash_size();
unsigned get_ext_rom_offset();
unsigned get_ext_rom_size();

int read_vptable(struct vpartitions *output);
int write_vptable(const struct vpartitions *input);

unsigned vpart_available_size();
int vpart_variable_exist();
bool vpart_partition_exist(const char* pName);
int vpart_partition_size(const char* pName);
int vpart_partition_order(const char* pName);
int mirrorpart_partition_order(const char* pName);
void vpart_resize_asize();
short vpart_variable_size();
void vpart_add(const char *pData);
void vpart_add_ex(const char *pName, unsigned size);
void vpart_restruct();
void vpart_del(const char *pName);
void vpart_list();
void vpart_clear();
void vpart_create_default();
void init_vpart();
void vpart_commit();
void vpart_read();
void vpart_enable_extrom();
void vpart_disable_extrom();
void vpart_resize(const char *pData);
void vpart_resize_ex(const char *pName, unsigned size);
