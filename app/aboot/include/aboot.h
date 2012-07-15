#ifndef _ABOOT_H_
#define _ABOOT_H_

#include <dev/keys.h>

#define EXPAND(NAME) 	#NAME
#define TARGET(NAME) 	EXPAND(NAME)
#define DEFAULT_CMDLINE "";

#define FASTBOOT_MODE   0x77665500
#define ANDRBOOT_MODE	0x77665501
#define RECOVERY_MODE   0x77665502

#define ERR_KEY_CHANGED 99
#define MAX_MENU 	16

#define ROUND_TO_PAGE(x,y) (((x) + (y)) & (~(y)))

unsigned page_size = 0;
unsigned page_mask = 0;
void *target_get_scratch_address(void);
void reboot_device(unsigned reboot_reason);
void htcleo_boot();
unsigned target_pause_for_battery_charge(void);
void target_battery_charging_enable(unsigned enable, unsigned disconnect);
void display_shutdown(void);
void shutdown(void);
void display_init(void);
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
int _bad_blocks;
struct flash_info *flash_info;
struct ptable flash_devinfo;
unsigned boot_into_sboot = 0;
unsigned boot_into_tboot = 0;
unsigned board_machtype(void);
unsigned check_reboot_mode(void);
unsigned* target_atag_mem(unsigned* ptr);
uint16_t keys[] = { KEY_VOLUMEUP, KEY_VOLUMEDOWN, KEY_SOFT1, KEY_SEND, KEY_CLEAR, KEY_BACK, KEY_HOME };
uint16_t keyp = ERR_KEY_CHANGED;
static const char *battchg_pause = " androidboot.mode=offmode_charging";
static unsigned char buf[4096]; //Equal to max-supported pagesize

#define ON 		1
#define OFF	 	0
unsigned run_usb_listener = 1;
void set_usb_status_listener(int state);

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

struct menu 
{
	int maxarl;
	int selectedi;
	int goback;
	int top_offset;
	int bottom_offset;
	char MenuId[80];
	char backCommand[64];
	char data[64];
	struct menu_item item[MAX_MENU];
};

struct menu main_menu;		// MAIN MENU
struct menu sett_menu;		// SETTINGS
struct menu rept_menu;		// RESIZE PARTITIONS
struct menu about_menu; 	// INFO
struct menu cred_menu;  	// CREDITS
struct menu help_menu;  	// HELP
struct menu rear_menu;  	// REARRANGE PARTITIONS
struct menu cust_menu;
struct menu *active_menu;

#endif
