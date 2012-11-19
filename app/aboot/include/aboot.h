#ifndef _ABOOT_H_
#define _ABOOT_H_

#include <dev/keys.h>

#define ROUND_TO_PAGE(x,y)	(((x) + (y)) & (~(y)))
#define round(x)			((long)((x)+0.5))
#define TIMER_HANDLES_KEY_PRESS_EVENT	0
#define TIMER_HANDLES_AUTOBOOT_EVENT	0

unsigned page_size = 0;
unsigned page_mask = 0;
void draw_clk_header(void);
void redraw_menu(void);
void prnt_nand_stat(void);
void cmd_flashlight(void);
void cmd_powerdown(const char *arg, void *data, unsigned sz);
void cmd_dmesg(const char *arg, void *data, unsigned sz);
void cmd_oem_boot_rec(void);
void cmd_oem_credits(void);
void oem_help(void);
int boot_linux_from_flash(void);
struct flash_info *flash_info;
struct ptable flash_devinfo;
static const char *battchg_pause = " androidboot.mode=offmode_charging";
static unsigned char buf[4096];
char command_to_execute[32];
uint16_t keys[] = { KEY_VOLUMEUP,
					KEY_VOLUMEDOWN,
					KEY_SOFT1,
					KEY_SEND,
					KEY_POWER/*KEY_CLEAR*/,
					KEY_BACK,
					KEY_HOME };
bool run_usbcheck;
bool key_listen;
bool usb_stat;
time_t suspend_time;
int msm_microp_i2c_status;
int htcleo_panel_status;

struct atag_ptbl_entry
{
	char name[16];
	unsigned offset;
	unsigned size;
	unsigned flags;
};

struct menu_item 
{
	char mTitle[64];
	char command[64];
	int x;
	int y;
};

#define MAX_MENU 	18
struct menu 
{
	int maxarl;
	int selectedi;
	int goback;
	int top_offset;
	int bottom_offset;
	char MenuId[80];
	int id;
	char backCommand[64];
	char data[64];
	struct menu_item item[MAX_MENU];
};

// MAIN MENU
#define MAIN_MENU_ID		1
struct menu main_menu;
// SETTINGS
#define SETT_MENU_ID		2
struct menu sett_menu;
// RESIZE PARTITIONS
#define REPT_MENU_ID		3
struct menu rept_menu;
// RESIZE PARTITIONS SUB
#define REPT_SUB_MENU_ID	3
struct menu rept_sub_menu;
// REARRANGE PARTITIONS
#define REAR_MENU_ID		4
struct menu rear_menu;
// REARRANGE PARTITIONS SUB
#define REAR_SUB_MENU_ID	4
struct menu rear_sub_menu;
// INFO
#define ABOUT_MENU_ID		5
struct menu about_menu;
// SHARED
#define SHARED_MENU_ID		6
struct menu shared_menu;
// ACTIVE MENU POINTER
struct menu *active_menu;

int show_multi_boot_screen;
unsigned selected_boot;

struct boot_parts {
	char name[32];
};

struct boot_parts supported_boot_partitions[] =
{
	{"boot",},
	{"sboot"},
	{"tboot"},
	{"vboot"},
	{"wboot"},
	{"xboot"},
	{"yboot"},
	{"zboot"},
	{""},
};

#endif