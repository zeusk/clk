/*
 * Copyright (c) 2009, Google Inc.
 * All rights reserved.
 *
 * Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
 *
 * Copyright (c) 2011, Rick@xda-developers.com, All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Code Aurora nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include <aboot.h>
#include <bootimg.h>
#include <bootreason.h>
#include <board.h>
#include <fastboot.h>
#include <recovery.h>
#include <version.h>
#include <app.h>
#include <array.h>
#include <bits.h>
#include <compiler.h>
#include <debug.h>
#include <err.h>
#include <platform.h>
#include <target.h>
#include <reg.h>
#include <smem.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <arch/arm.h>
#include <arch/ops.h>
#include <dev/ds2746.h>
#include <dev/flash.h>
#include <dev/fbcon.h>
#include <dev/udc.h>
#include <dev/gpio.h>
#include <kernel/thread.h>
#include <lib/ptable.h>
#include <lib/devinfo.h>
#include <lib/fs.h>
#include <sys/types.h>
#include <target/acpuclock.h>
#include <target/hsusb.h>
#include <target/board_htcleo.h>
#include <platform/gpio.h>
#include <platform/iomap.h>
#include <platform/timer.h>
#include <platform/interrupts.h>

char charVal(char c)
{
	c&=0xf;
	if(c<=9) return '0'+c;
	
	return 'A'+(c-10);
}

int str2u(const char *x)
{
	while(*x==' ')x++;

	unsigned base=10;
	int sign=1;
    unsigned n = 0;

    if(strstr(x,"-")==x) { sign=-1; x++;};
    if(strstr(x,"0x")==x) {base=16; x+=2;}

    while(*x) {
    	char d=0;
        switch(*x) {
        	case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
            	d = *x - '0';
            	break;
        	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
            	d = (*x - 'a' + 10);
            	break;
       		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
				d = (*x - 'A' + 10);
				break;
        	default:
				return sign*n;
        }
        if(d>=base) return sign*n;
        n*=base;n+=d;
        x++;
    }
	
	return sign*n;
}

void hex2ascii(char *hexstr, char *asciistr)
{
	int newval;
	for (int i = 0; i < (int)(strlen(hexstr) - 1); i+=2) {
		int firstvalue = hexstr[i] - '0';
		int secondvalue;
		switch(hexstr[i+1]) {
			case 'A': case 'a':
				secondvalue = 10;
				break;
			case 'B': case 'b':
				secondvalue = 11;
				break;
			case 'C': case 'c':
				secondvalue = 12;
				break;
			case 'D': case 'd':
				secondvalue = 13;
				break;
			case 'E': case 'e':
				secondvalue = 14;
				break;
			case 'F': case 'f':
				secondvalue = 15;
				break;
			default: 
				secondvalue = hexstr[i+1] - '0';
				break;
		}
		newval = ((16 * firstvalue) + secondvalue);
		*asciistr = (char)(newval);
		asciistr++;
	}
}

void dump_mem_to_buf(char* start, int len, char *buf)
{
	while(len>0) {
		int slen = len > 29 ? 29 : len;
		for(int i=0; i<slen; i++) {
			buf[i*2] = charVal(start[i]>>4);
			buf[i*2+1]= charVal(start[i]);
		}
		buf[slen*2+1]=0;
		start+=slen;
		len-=slen;
	}
}

/*
 * koko : Get device IMEI
 */
char device_imei[64];
void update_device_imei(void)
{
	char imeiBuffer[64];
	dump_mem_to_buf((char*)str2u("0x1EF260"), str2u("0xF"), imeiBuffer);

	hex2ascii(imeiBuffer, device_imei);
	
	return;
}

/*
 * koko : Get device CID
 */
char device_cid[24];
void update_device_cid(void)
{
	char cidBuffer[24];
	dump_mem_to_buf((char*)str2u("0x1EF270"), str2u("0x8"), cidBuffer);

	hex2ascii(cidBuffer, device_cid);

	return;
}

/*
 * koko : Get radio version
 */
char radio_version[25];
char radBuffer[25];
void update_radio_ver(void)
{
	while(radBuffer[0] != '3')
		dump_mem_to_buf((char*)str2u("0x1EF220"), str2u("0xA"), radBuffer);

	hex2ascii(radBuffer, radio_version);

	return;
}

/*
 * koko : Get spl version
 */
char spl_version[24];
void update_spl_ver(void)
{
	char splBuffer[24];
	dump_mem_to_buf((char*)str2u("0x1004"), str2u("0x9"), splBuffer);

	hex2ascii(splBuffer, spl_version);

	return;
}

/*
 * koko : Get device BT Mac-Addr
 */
char device_btmac[24];
void update_device_btmac(void)
{
	char btmacBuffer[24];
	dump_mem_to_buf((char*)str2u("0x82108"), str2u("0x6"), btmacBuffer);

	sprintf( device_btmac, "%c%c:%c%c:%c%c:%c%c:%c%c:%c%c",
			btmacBuffer[10], btmacBuffer[11], btmacBuffer[8], btmacBuffer[9],
			btmacBuffer[6], btmacBuffer[7], btmacBuffer[4], btmacBuffer[5],
			btmacBuffer[2], btmacBuffer[3], btmacBuffer[0], btmacBuffer[1] );
	
	return;
}

/*
 * koko : Get device WiFi Mac-Addr
 */
char device_wifimac[24];
void update_device_wifimac(void)
{	/*
	 * koko: If we install LK right after
	 *		 having flashed a WM ROM the
	 *		 wifi mac addr is read from '0xFC028', 
	 *		 BUT after flashing anything else
	 *		 we get only FFs. (?)
	 */
	char wifimacBuffer[24];
	dump_mem_to_buf((char*)str2u("0xFC028"), str2u("0x6"), wifimacBuffer);

	sprintf( device_wifimac, "%c%c:%c%c:%c%c:%c%c:%c%c:%c%c",
			wifimacBuffer[0], wifimacBuffer[1], wifimacBuffer[2], wifimacBuffer[3],
			wifimacBuffer[4], wifimacBuffer[5], wifimacBuffer[6], wifimacBuffer[7],
			wifimacBuffer[8], wifimacBuffer[9], wifimacBuffer[10], wifimacBuffer[11] );
	
	return;
}

/*
 * koko : Changed the header to display
 *		
 *	eg: 	cLK 1.7.0.0   |   SPL 2.08.HSPL   |   RADIO 2.15.50.14
 *		Black Edition
 *					NAND flash chipset :  Hynix-BC
 *			           Available/Total size(MB): 445 / 512
 *				         	     ExtROM:  DISABLED
 */
void draw_clk_header(void)
{
	//Header will always be black with white text no matter if inverted or not
	fbcon_setfg(0xffff);
	fbcon_settg(0x0000);
	if(show_multi_boot_screen==0){
		printf("   cLK %s   |   SPL %s   |   RADIO %s   ", PSUEDO_VERSION, spl_version, radio_version);
		printf("                                                            ");
		printf("                          CPU clock freq(KHz):     %6u   ", acpuclk_get_rate());
		printf("                           NAND flash chipset: %7s-%02X   ", flash_info->manufactory, flash_info->device);
		printf("                     Available/Total size(MB): %s%4i%s%4i   ",
				((int)( flash_info->num_blocks / get_blk_per_mb() ) == 512 ? "" : " "),
				(int)( get_usable_flash_size() / get_blk_per_mb() ),
				((int)( flash_info->num_blocks / get_blk_per_mb() ) == 512 ? " /" : "/"),
				(int)( flash_info->num_blocks / get_blk_per_mb() ));
		printf("                                       			ExtROM:   %s",
				(device_info.extrom_enabled ? " ENABLED   " : "DISABLED   "));
	}else{
		printf(  "\n __________________________________________________________ \
					|                                                          |");
		printf(	   "|  (L) i t t l e - (K) e r n e l      b o o t L o a d e r  |\
					|__________________________________________________________|\n");
	}

	//From this point set colors according to inverted
	fbcon_setfg((inverted ? 0x0000 : 0xffff));
	fbcon_settg((inverted ? 0xffff : 0x0000));
	if (active_menu != NULL) {
		_dputs("<> ");
		_dputs(active_menu->MenuId);
		_dputs("\n\n");
	}
}

/*
 * koko : Selected item gets
 *	  different foregroung color
 *	  to indicate it is selected - No highlight
 */
void selector_disable(void)
{
	fbcon_set_x_cord(active_menu->item[active_menu->selectedi].x);
	fbcon_set_y_cord(active_menu->item[active_menu->selectedi].y);
	fbcon_forcetg(true);
	fbcon_reset_colors_rgb555();
	_dputs(active_menu->item[active_menu->selectedi].mTitle);
	fbcon_forcetg(false);
}

void selector_enable(void)
{
	if(active_menu->maxarl > 0) // if there are no sub-entries don't enable selection
	{
		fbcon_set_x_cord(active_menu->item[active_menu->selectedi].x);
		fbcon_set_y_cord(active_menu->item[active_menu->selectedi].y);
		fbcon_setfg((inverted ? 0x001f : 0x65ff));
		_dputs(active_menu->item[active_menu->selectedi].mTitle);
		fbcon_reset_colors_rgb555();
	}
}

/*
 * koko : Add menu item at specific index
 */
void add_menu_item(struct menu *xmenu,const char *name,const char *command,const int index)
{
	if( (xmenu->maxarl==MAX_MENU) || (index>MAX_MENU) ) {
		printf("Menu: is overloaded with entry %s",name);
		return;
	}
	if(index>xmenu->maxarl) {
      	strcpy(xmenu->item[index].mTitle,name);
      	strcpy(xmenu->item[index].command,command);
	} else {
		for (int i = (xmenu->maxarl+1); i > (index); i--) {
	      	strcpy(xmenu->item[i].mTitle, xmenu->item[i-1].mTitle);
	      	strcpy(xmenu->item[i].command, xmenu->item[i-1].command);
	    }
	    strcpy(xmenu->item[index].mTitle,name);
      	strcpy(xmenu->item[index].command,command);
	}

    xmenu->item[xmenu->maxarl].x = 0;
    xmenu->item[xmenu->maxarl].y = 0;
    xmenu->maxarl++;

	return;
}

/*
 * koko : Remove menu item at specific index
 */
void rem_menu_item(struct menu *xmenu,const int index)
{
	for (int i = (index); i < (xmenu->maxarl); i++) {
      	strcpy(xmenu->item[i].mTitle, xmenu->item[i+1].mTitle);
      	strcpy(xmenu->item[i].command, xmenu->item[i+1].command);
    }

    xmenu->item[xmenu->maxarl].x = 0;
    xmenu->item[xmenu->maxarl].y = 0;
    xmenu->maxarl--;

	return;
}

/*
 * koko : Change menu item's name and command
 */
void change_menu_item(struct menu *xmenu,const char *oldname,const char *newname,const char *newcommand)
{
    for (int i = 0; i < xmenu->maxarl; i++) {
      	if (!memcmp(xmenu->item[i].mTitle, oldname, strlen(xmenu->item[i].mTitle))) {
      		strcpy(xmenu->item[i].mTitle, newname);
      		strcpy(xmenu->item[i].command, newcommand);
      	}
    }
	return;
}

/*
 * koko : Basic check for BOOT_MAGIC in image header
 *
int img_hdr_is_valid(struct ptentry *ptn)
{
	struct boot_img_hdr *hdr = (void*) buf;

	if(flash_read(ptn, 0, buf, page_size))
		return 0;

	if(memcmp(hdr->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE))
		return 0;

	return 1;
}
 */

/*
 * koko: Monitor usb status
 */
static int usb_listener(void *arg)
{
	while (run_usbcheck) {
		for (int i=0; i<24; i++) {
			if (run_usbcheck) // Check every 125ms the value of run_usbcheck
				mdelay(125);  // 24 * 125 = 3000ms total delay time if run_usbcheck is not set to 0
			else
				goto usb_listener_done;
		}
			
		// User might have switched from host pc to wall charger or unplugged the cable.
		// So keep checking every time we are in the loop
		usb_charger_change_state();
		
		// Turn led according to charger state
		if ( usb_charger_state() == CHG_USB_HIGH || usb_charger_state() == CHG_AC || usb_charger_state() == CHG_USB_LOW) {
			// Solid amber
			if (htcleo_notif_led_mode != 2)
				thread_resume(thread_create("htcleo_notif_led_set_mode_2",
											&htcleo_notif_led_set_mode,
											(void *)2,
											HIGH_PRIORITY,
											DEFAULT_STACK_SIZE));
		} else if (usb_charger_state() == CHG_OFF_FULL_BAT) {
			// Solid green
			if (htcleo_notif_led_mode != 1)
				thread_resume(thread_create("htcleo_notif_led_set_mode_1",
											&htcleo_notif_led_set_mode,
											(void *)1,
											HIGH_PRIORITY,
											DEFAULT_STACK_SIZE));
		} else /* if (usb_charger_state() == CHG_OFF) */ {
			// Off
			if (htcleo_notif_led_mode != 0)
				thread_resume(thread_create("htcleo_notif_led_set_off",
											&htcleo_notif_led_set_mode,
											(void *)0,
											HIGH_PRIORITY,
											DEFAULT_STACK_SIZE));
		}
	}
	
usb_listener_done:
	if (htcleo_notif_led_mode != 0)
		thread_resume(thread_create("htcleo_notif_led_set_off",
									&htcleo_notif_led_set_mode,
									(void *)0,
									HIGH_PRIORITY,
									DEFAULT_STACK_SIZE));
	thread_exit(0);
	return 0;
}

/*
 * koko: Copy the image of one partition to another
 *
int flash_copy(struct ptentry *source_ptn, struct ptentry *target_ptn)
{
	if(source_ptn == NULL || target_ptn == NULL) {
		return 0;
	}
	if(source_ptn->length > target_ptn->length) {
		printf( "   WARNING:  '%s' larger from '%s'!!!\n   The real written image of '%s' may not fit to '%s'\n",
				source_ptn->name, target_ptn->name, source_ptn->name, target_ptn->name);
		//return 0;
	}
	
	unsigned char source_img[source_ptn->length*0x20000];
	
	printf( "   Reading %i bytes from '%s'...", source_ptn->length*0x20000, source_ptn->name);
	if(flash_read(source_ptn, 0, (void *)source_img, (source_ptn->length)*0x10000)) {
		printf("Error!\n");
		source_img[0] = '\0';
		return 0;
	} else {
		printf("Done!\n");
	}

	printf( "   Writing %i bytes to '%s'...", source_ptn->length*0x20000, target_ptn->name);
	if(flash_write(target_ptn, 0, (void *)source_img, target_ptn->length*0x20000)) {
		printf("Error!\n");
		source_img[0] = '\0';
		return 0;
	} else {
		printf("Done\n");	
	}
	
	source_img[0] = '\0';
	
	return 1;
}
 */
 
/*
 * koko: Exchange images between two partitions
 *
int flash_exchange(struct ptentry *ptn_a, struct ptentry *ptn_b)
{
	if(ptn_a == NULL || ptn_b == NULL) {
		return 0;
	}
	if(ptn_a->length != ptn_b->length) {
		printf( "   WARNING:  '%s' and '%s' have different sizes\n", ptn_a->name, ptn_b->name);
		//return 0;
	}
	
	printf( "   Reading partitions...\n");
	unsigned char source_img_a[ptn_a->length*0x20000];
	if(flash_read(ptn_a, 0, (void *)source_img_a, ptn_a->length*0x10000)) {
		printf("   ERROR: Cannot read from '%s'!!!\n", ptn_a->name);
		source_img_a[0] = '\0';
		return 0;
	} else {
		printf("   Image from '%s' read\n", ptn_a->name);
	}
	unsigned char source_img_b[ptn_b->length*0x20000];
	if(flash_read(ptn_b, 0, (void *)source_img_b, ptn_b->length*0x10000)) {
		printf("   ERROR: Cannot read from '%s'!!!\n", ptn_b->name);
		source_img_a[0] = '\0';
		source_img_b[0] = '\0';
		return 0;
	} else {
		printf("   Image from '%s' read\n", ptn_b->name);	
	}

	printf( "   Switching partitions...\n");
	if(flash_write(ptn_a, 0, (void *)source_img_b, ptn_a->length*0x20000)) {
		printf("   ERROR: Cannot write to '%s'!!!\n", ptn_a->name);
		source_img_a[0] = '\0';
		source_img_b[0] = '\0';
		return 0;
	} else {
		printf("   Image copied from '%s' to '%s'\n", ptn_b->name, ptn_a->name);	
	}
	if(flash_write(ptn_b, 0, (void *)source_img_a, ptn_b->length*0x20000)) {
		printf("   ERROR: Cannot write to '%s'!!!\n", ptn_b->name);
		source_img_a[0] = '\0';
		source_img_b[0] = '\0';
		return 0;
	} else {
		printf("   Image copied from '%s' to '%s'\n", ptn_a->name, ptn_b->name);	
	}
	
	source_img_a[0] = '\0';
	source_img_b[0] = '\0';
	
	return 1;	
} 
 */

/*
void test_func()
{
	struct ptable *ptable = flash_get_ptable();
	struct ptentry *ptn_a = ptable_find(ptable, "boot");
	struct ptentry *ptn_b = ptable_find(ptable, "sboot");
	//if(!flash_exchange(ptn_a, ptn_b)) {
	//	printf("   Aborted !\n");
	//}else{
	//	printf("   Done !\n");
	//}
	if(!flash_copy(ptn_a, ptn_b)) {
		printf("   Aborted !\n");
	}else{
		printf("   Done !\n");
	}
}
 */
 
/*
 * koko: Get number of standard partitions that are still present 
 */
short standard_ptn_present;
void get_device_standard_ptn_num()
{
	/* koko: Start counting from 5 because the partitions:
	 *		*****************************************************
	 * 		* lk		!										*
	 *   	* recovery	!										*
	 * 		* boot		! Already one of the boot partitions !	*
	 *		* misc		!										*
	 *		* cache		? Used by recovery for log ?			*
	 *		*****************************************************
	 *		 are required and should not be erased.
	 */
	standard_ptn_present = 5;

	//In case the user doesn't have a NAND installation and
	//has erased 'userdata' and 'system' check just for those parts
	//so that it would be possible to add two more boot partitions
	standard_ptn_present += ( device_partition_exist("system")
							+ device_partition_exist("userdata"));

	return;
}

/*
 * koko: Check if 'userdata' is present.
 *		 If not set 'boot' partition as donor
 *		 from which extra boot partitions will take space,
 *		 and which is used when en/disabling ExtRom 
 */
char *get_partition_donor()
{
	return (standard_ptn_present == 5 ? "boot" : "userdata");
}

// MAIN MENU
void eval_main_menu(char *command)
{
	char* subCommand = NULL;
	// BOOT
	if (!memcmp(command, "boot_", 5 ))
	{
		subCommand = command + 5;
		active_menu=NULL;
		fbcon_resetdisp();
		if(inverted){draw_clk_header();fbcon_settg(0xffff);fbcon_setfg(0x0000);printf(" \n\n");}
		// BOOT RECOVERY
		if ( !memcmp( subCommand, "recv", strlen( subCommand ) ) )
		{
			boot_into_recovery = 1;
		} else
		// BOOT ANDROID @ %x%BOOT
		{
			boot_into_recovery = 0;
			selected_boot = (unsigned)atoi( subCommand );
		}
		//htcleo_ts_deinit();
		boot_linux_from_flash();
		return;
	}
	// FLASHLIGHT
	else if (!memcmp(command,"init_flsh", strlen(command)))
	{
		redraw_menu();
		cmd_flashlight();
		selector_enable();
		return;
	}
	// SETTINGS
	else if (!memcmp(command,"goto_sett", strlen(command)))
	{
		/* koko : Need to link mirror_info with device_info for option "REARRANGE PARTITIONS" to work.
				  The linking is done here so that every time the user enters this submenu
				  he resets mirror_info to device_info. Check devinfo.h for new struct */
		strcpy( mirror_info.tag, "MIRRORINFO" );
		for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
		{
			if(strlen( device_info.partition[i].name ) != 0)
			{
				strcpy( mirror_info.partition[i].name, device_info.partition[i].name );
				mirror_info.partition[i].size = device_info.partition[i].size;
				mirror_info.partition[i].asize = device_info.partition[i].asize;
				mirror_info.partition[i].order = i+1;
			}
		}
		mirror_info.extrom_enabled = device_info.extrom_enabled;
		mirror_info.fill_bbt_at_start = device_info.fill_bbt_at_start;
		mirror_info.inverted_colors = device_info.inverted_colors;
		mirror_info.show_startup_info = device_info.show_startup_info;
		mirror_info.usb_detect = device_info.usb_detect;
		mirror_info.cpu_freq = device_info.cpu_freq;
		mirror_info.boot_sel = device_info.boot_sel;
		mirror_info.multi_boot_screen = device_info.multi_boot_screen;
		mirror_info.panel_brightness = device_info.panel_brightness;
		mirror_info.udc = device_info.udc;
		mirror_info.use_inbuilt_charging = device_info.use_inbuilt_charging;

		active_menu = &sett_menu;
		redraw_menu();
		selector_enable();
		return;
    }
	// POWER
	else if (!memcmp(command, "acpu_", 5 ))
	{
		subCommand = command + 5;
		// REBOOT
		if ( !memcmp( subCommand, "ggwp", strlen( subCommand ) ) )
		{
			target_reboot(0);
		} else
		// REBOOT cLK
		if ( !memcmp( subCommand, "bgwp", strlen( subCommand ) ) )
		{
			target_reboot(FASTBOOT_MODE);
		} else
		// POWERDOWN
		if ( !memcmp( subCommand, "pawn", strlen( subCommand ) ) )
		{
			target_shutdown();
		}
		return;
	}
	// INFO
	else if (!memcmp(command,"goto_about", strlen(command)))
	{
		active_menu = &about_menu;
		redraw_menu();
		selector_enable();
		return;
    }
	else
	{
		subCommand = NULL;
		redraw_menu();
		printf("   HBOOT BUG: Somehow fell through main_menu_command()\n");
		selector_enable();
		return;
	}
}

// SETTINGS MENU
void eval_sett_menu(char *command)
{
	char* subCommand = NULL;
	// RESIZE PARTITIONS
	if (!memcmp(command,"goto_rept", strlen(command)))
	{
		rept_menu.top_offset	= 0;
		rept_menu.bottom_offset	= 0;
		rept_menu.maxarl		= 0;
		rept_menu.goback		= 0;

		char cmd[64];
      	char partname[64];
		int k=0;
      	for ( unsigned i = 0; i < MAX_NUM_PART; i++ )
      	{
      		if( strlen(device_info.partition[i].name)!=0 )
			{
	      		strcpy( cmd, "rept_" );
	      		strcat( cmd, device_info.partition[i].name );
	      		sprintf( partname, "   %s", device_info.partition[i].name);
	      		add_menu_item(&rept_menu, partname, cmd, k++);
			}
      	}
        add_menu_item(&rept_menu, "   PRINT PARTITION TABLE", "prnt_stat", k++);
        add_menu_item(&rept_menu, "   COMMIT CHANGES", "rept_commit", k);

		active_menu = &rept_menu;
		redraw_menu();
		selector_enable();
		return;
    }
	// REARRANGE PARTITIONS
	else if (!memcmp(command,"goto_rear", strlen(command)))
	{
		rear_menu.top_offset	= 0;
		rear_menu.bottom_offset	= 0;
		rear_menu.maxarl		= 0;
		rear_menu.goback		= 0;

		char cmd[64];
      	char partname[64];
		int k=0;
		for ( unsigned i = 1; i < MAX_NUM_PART+1; i++ )
		{
			for ( unsigned j = 0; j < MAX_NUM_PART; j++ )
			{
				if( ((int)i==mirror_info.partition[j].order) && (strlen( mirror_info.partition[j].name ) != 0) )
				{
					strcpy( cmd, "rear_" );
	      	      			strcat( cmd, mirror_info.partition[j].name );
	      	      			sprintf( partname, "   %s", mirror_info.partition[j].name);
	      	      			add_menu_item(&rear_menu, partname, cmd, k++);
				}
			}
		}
            	add_menu_item(&rear_menu, "   PRINT PARTITION TABLE", "prnt_mirror_info", k++);
            	add_menu_item(&rear_menu, "   COMMIT CHANGES", "rear_commit", k);

		active_menu = &rear_menu;
      	redraw_menu();
		selector_enable();
		return;
    }
	// FORMAT NAND (use shared_menu for confirmation dialog)
	else if (!memcmp(command,"format_nand", strlen(command)))
	{
		shared_menu.top_offset		= 0;
		shared_menu.bottom_offset	= 0;
		shared_menu.selectedi		= 1;// CANCEL by default
		shared_menu.maxarl			= 0;
		shared_menu.goback			= 0;

		strcpy( shared_menu.MenuId, "FORMAT NAND" );
		strcpy( shared_menu.backCommand, "goto_sett" );

		add_menu_item(&shared_menu, "   APPLY", "frmt_1", 0);
		add_menu_item(&shared_menu, "   CANCEL", "frmt_0", 1);

		active_menu = &shared_menu;
		redraw_menu();
		selector_enable();
		return;
    }
#if 0
	// FLASH LEOREC.IMG
	else if (!memcmp(command,"flash_recovery", strlen(command)))
	{
		redraw_menu();
		printf( "\n   Initializing flash process..." );

		// Load recovery image from sdcard
		fs_init();
		printf( "\n   fs_init() done!" );
		void *buf = NULL;
		fs_load_file("/LEOREC.img", buf);
		fs_stop();

		// Format recovery partition
		struct ptable *ptable;
		ptable = flash_get_ptable();
		if(ptable == NULL) {
			printf( "\n   ERROR: PTABLE not found!!!");
			return;
		}
		for ( int i = 0; i < ptable_size(ptable); i++ ) {
			if(!memcmp(ptable_get(ptable, i)->name,"recovery", strlen(ptable_get(ptable, i)->name))) {
				printf( "\n   Formatting %s...", ptable_get(ptable, i)->name );
				flash_erase(ptable_get(ptable, i));
				printf("\n   Format completed !");
			}
		}

		// Write recovery partition
		struct ptentry *recptn;
		recptn = ptable_find(ptable, "recovery");
		if(recptn == NULL) {
			printf("\n   ERROR: No recovery partition found!!!");
			return;
		}
		//int flash_write(struct ptentry *ptn, unsigned extra_per_page, const void *data, unsigned bytes);
		if(flash_write(recptn, 0, (void*)buf, (sizeof(buf)))) {
			printf("\n   ERROR: Cannot write partition!!!");
			return;
		}
		printf("\n   Recovery updated !\n");

		free(buf);

		fs_stop();
		selector_enable();
		return;
	}
#endif
	// EXTRA BOOT
	else if (!memcmp(command, "xboot_", 6 ))
	{
		// first check for the standard partitions number
		// and set the standard_ptn_present
		get_device_standard_ptn_num();
		char menuTitle[64];
		char newmenuTitle[64];
		char menuCommand[64];
		// set donor partition according to standard_ptn_present
		char *pdonor = get_partition_donor();
		int idx;
		subCommand = command + 6;
		// REMOVE EXTRA BOOT
		if ( !memcmp( subCommand, "dis_", 4 ) )
		{
			idx = atoi( subCommand + 4 );
			if (idx == 1) {
				sprintf( menuTitle, "   REMOVE %s",
					supported_boot_partitions[idx].name);
				sprintf( newmenuTitle, "   ADD %s",
						supported_boot_partitions[idx].name);
				sprintf( menuCommand, "xboot_en_%i", idx);
				change_menu_item(&sett_menu, menuTitle, newmenuTitle, menuCommand);
				rem_menu_item(&sett_menu, 3);
			} else {
				sprintf( menuTitle, "   REMOVE %s",
					supported_boot_partitions[idx].name);
				sprintf( newmenuTitle, "   REMOVE %s",
						supported_boot_partitions[idx-1].name);
				sprintf( menuCommand, "xboot_dis_%i", idx-1);
				change_menu_item(&sett_menu, menuTitle, newmenuTitle, menuCommand);
				
				sprintf( menuTitle, "   ADD %s",
					supported_boot_partitions[(idx==7 ? idx : idx+1)].name);
				sprintf( newmenuTitle, "   ADD %s",
						supported_boot_partitions[idx].name);
				sprintf( menuCommand, "xboot_en_%i", idx);
				if (idx == 7) {
					add_menu_item(&sett_menu, menuTitle, menuCommand, 3);
				} else {
					change_menu_item(&sett_menu, menuTitle, newmenuTitle, menuCommand);
				}
			}
			
			// Use the freed space for donor by default
			int new_data_size=(int)( device_partition_size(pdonor) + device_partition_size(supported_boot_partitions[idx].name) );
			//flash_erase(ptable_find(flash_get_ptable(), supported_boot_partitions[idx].name));
			device_del(supported_boot_partitions[idx].name);
			if(!mirror_info.partition[device_partition_order(pdonor)-1].asize)
			{
				device_resize_ex(pdonor, new_data_size, true); // if donor has a fixed size
			}
			if(device_info.boot_sel == idx){
				device_info.boot_sel = 0;
			}
			active_menu = &main_menu;
			if(active_menu->maxarl == (8+idx)){rem_menu_item(&main_menu, idx);}
			active_menu = &sett_menu;
		} else
		// ADD EXTRA BOOT
		if ( !memcmp( subCommand, "en_", 3 ) )
		{
			idx = atoi( subCommand + 3 );
			if ((unsigned)(idx + standard_ptn_present - 1) < MAX_NUM_PART) {
				if (idx == 1) {
					sprintf( menuTitle, "   ADD %s",
							supported_boot_partitions[idx].name);
					sprintf( newmenuTitle, "   REMOVE %s",
							supported_boot_partitions[idx].name);
					sprintf( menuCommand, "xboot_dis_%i", idx);
					change_menu_item(&sett_menu, menuTitle, newmenuTitle, menuCommand);
				
					sprintf( menuTitle, "   ADD %s",
							supported_boot_partitions[idx+1].name);
					sprintf( menuCommand, "xboot_en_%i", idx+1);
					if( strlen( supported_boot_partitions[idx+1].name ) != 0 )
						add_menu_item(&sett_menu, menuTitle, menuCommand, 3);
				} else {
					sprintf( menuTitle, "   REMOVE %s",
							supported_boot_partitions[idx-1].name);
					sprintf( newmenuTitle, "   REMOVE %s",
							supported_boot_partitions[idx].name);
					sprintf( menuCommand, "xboot_dis_%i", idx);
					change_menu_item(&sett_menu, menuTitle, newmenuTitle, menuCommand);
				
					sprintf( menuTitle, "   ADD %s",
							supported_boot_partitions[idx].name);
					sprintf( newmenuTitle, "   ADD %s",
							supported_boot_partitions[idx+1].name);
					sprintf( menuCommand, "xboot_en_%i", idx+1);
					if( strlen( supported_boot_partitions[idx+1].name ) != 0 ) {
						change_menu_item(&sett_menu, menuTitle, newmenuTitle, menuCommand);
					} else {
						rem_menu_item(&sett_menu, 3);
					}
				}

				char name_and_size[64];
				bool this_boot_was_present = device_partition_exist(supported_boot_partitions[idx].name);
				device_clear();
				for ( int i = 0; i < MAX_NUM_PART; i++ ) {
					if(strlen( mirror_info.partition[i].name ) != 0) {
						if(memcmp(mirror_info.partition[i].name, pdonor, strlen(mirror_info.partition[i].name))) {
							sprintf( name_and_size, "%s:%d:b", mirror_info.partition[i].name, mirror_info.partition[i].size );
							device_add(name_and_size);						
						} else {
							// Create xboot after donor by taking 5MB from it
							if(mirror_info.partition[i].asize) {
								sprintf( name_and_size, "%s:0", mirror_info.partition[i].name );
								device_add(name_and_size);  // if donor is auto-size
							} else {
								sprintf( name_and_size, "%s:%d:b", mirror_info.partition[i].name, (mirror_info.partition[i].size - 40) );
								device_add(name_and_size); // if donor has a fixed size
							}
							if(!this_boot_was_present) {
								sprintf( name_and_size, "%s:5", supported_boot_partitions[idx].name );
								device_add( name_and_size );
							}
						}
					}
				}
				sprintf( menuTitle, "   KERNEL @ %s [NULL]",
						supported_boot_partitions[idx].name);
				sprintf( menuCommand, "boot_%i", idx);
				add_menu_item(&main_menu, menuTitle, menuCommand, idx);
			} else {
				return;
			}
		}
		device_resize_asize();
		device_commit();
		redraw_menu();
		device_list();
			
		/* Load new PTABLE layout without rebooting */
		printf("\n       New PTABLE layout has been successfully loaded");
		htcleo_ptable_re_init();
		mdelay(2000);
		redraw_menu();
		selector_enable();
		return;
	}
	// EXTROM
	else if (!memcmp(command, "extrom_", 7 ))
	{
		// first check for the standard partitions number
		// and set the standard_ptn_present
		get_device_standard_ptn_num();
		// set donor partition according to standard_ptn_present
		char *pdonor = get_partition_donor();
		subCommand = command + 7;
		// Disabling ExtROM will automatically resize cache
		if ( !memcmp( subCommand, "disable", strlen( subCommand ) ) )
		{
			change_menu_item(&sett_menu, "   DISABLE ExtROM", "   ENABLE ExtROM", "extrom_enable");
			redraw_menu();

			/* Delete cache 											-> free		(cache size)		MB
			   Disable ExtROM											-> free		(cache size) - 24	MB
			   Resize donor by adding to it (((cache size) - 24) -5)MB	-> free			5				MB
											 reserving 5MB for cache
			   Add cache with 5MB size									-> free			0				MB
			*/
			int cachesize = device_partition_size("cache");
			int donorsize = device_partition_size(pdonor);

			//printf( "   Wiping cache..." );
			//flash_erase(ptable_find(flash_get_ptable(), "cache"));
			device_del( "cache" );

			printf( "   Disabling ExtROM..." );
			device_disable_extrom();

			if(cachesize>192)
			{
				if(cachesize-192>40){
				printf( "\n   Increasing donor ptn by %dMB...", (cachesize/get_blk_per_mb())-29 );
				}else{
				printf( "\n   Decreasing donor ptn by %dMB...", (cachesize/get_blk_per_mb())-29 );
				}
				if(!mirror_info.partition[device_partition_order(pdonor)-1].asize) // if donor has a fixed size
				{
					device_resize_ex( pdonor,  donorsize - 40 + (cachesize-192), true);
				}
				printf( "\n   Assigning last 5MB to cache...\n" );
				device_add( "cache:5" );
			}
			else
			{
				printf( "\n   Decreasing donor ptn by %dMB...", 5 );
				if(!mirror_info.partition[device_partition_order(pdonor)-1].asize) // if donor has a fixed size
				{
					device_resize_ex( pdonor,  donorsize - 40, true );
				}
				printf( "\n   Assigning last 5MB to cache...\n" );
				device_add( "cache:5" );
			}
		} else
		// Enabling ExtROM will automatically use the last 24 MB for cache
		if ( !memcmp( subCommand, "enable", strlen( subCommand ) ) )
		{
			change_menu_item(&sett_menu, "   ENABLE ExtROM", "   DISABLE ExtROM", "extrom_disable");
			redraw_menu();

			/* Delete cache 											-> free		(cache size)		MB
			   Resize donor by adding to it the freed (cache size)MB 	-> free		0			MB 
			   Enable ExtROM											-> free		24			MB
			   Add cache with 24MB size									-> free		0			MB
			*/
			int cachesize = device_partition_size( "cache" );
			int donorsize = device_partition_size(pdonor);

			//printf( "   Wiping cache..." );
			//flash_erase(ptable_find(flash_get_ptable(), "cache"));
			device_del( "cache" );

			printf( "   Increasing donor ptn by %dMB...", cachesize/get_blk_per_mb() );
			if(!mirror_info.partition[device_partition_order(pdonor)-1].asize) // if donor has a fixed size
			{
				device_resize_ex(pdonor, donorsize + cachesize, true);
			}
			printf( "\n   Enabling ExtROM..." );
			device_enable_extrom();

			printf( "\n   Assigning last 24MB of ExtROM to cache...\n" );
			device_add( "cache:192:b" );
		}
		device_resize_asize();
		device_commit();
		device_list();

		/* Load new PTABLE layout without rebooting */
		printf("\n       New PTABLE layout has been successfully loaded");
		htcleo_ptable_re_init();
		mdelay(2000);
		redraw_menu();
		selector_enable();
		return;
	}
	// STARTUP INFO
	if (!memcmp(command, "info_", 5 ))
	{
		int j = atoi( command + 5 );
		device_info.show_startup_info = j;
		device_commit();
		change_menu_item(&sett_menu,
						(j ? "   SHOW STARTUP INFO" : "   HIDE STARTUP INFO"),
						(j ? "   HIDE STARTUP INFO" : "   SHOW STARTUP INFO"),
						(j ? "info_0" : "info_1"));
		redraw_menu();
		selector_enable();
		return;
	}
 	// MULTIBOOT MENU
	else if (!memcmp(command, "multiboot_", 10 ))
	{
		if ( device_boot_ptn_num() ){
			int j = atoi( command + 10 );
			device_info.multi_boot_screen = j;
			device_commit();
			change_menu_item(&sett_menu,
							(j ? "   SHOW MULTIBOOT MENU" : "   HIDE MULTIBOOT MENU"),
							(j ? "   HIDE MULTIBOOT MENU" : "   SHOW MULTIBOOT MENU"),
							(j ? "multiboot_0" : "multiboot_1"));
			redraw_menu();
			selector_enable();
		}
		return;
	}
	// USB DETECT
    else if (!memcmp(command, "usbd_", 5 ))
	{
		int j = atoi( command + 5 );
		device_info.usb_detect = j;
		run_usbcheck = j;
		device_commit();
		change_menu_item(&sett_menu,
						(j ? "   ENABLE USB DETECTION" : "   DISABLE USB DETECTION"),
						(j ? "   DISABLE USB DETECTION" : "   ENABLE USB DETECTION"),
						(j ? "usbd_0" : "usbd_1"));
		if(j){		
			thread_resume(thread_create("usb_listener",
										&usb_listener,
										NULL,
										DEFAULT_PRIORITY,
										DEFAULT_STACK_SIZE));
		}else{
			//thread will just exit since we set run_usbcheck = 0
		}
		redraw_menu();
		selector_enable();
		return;
	}
	// INVERT SCREEN COLORS
 	else if (!memcmp(command,"invert_colors", strlen(command)))
	{
		inverted ^= 1;
		device_info.inverted_colors ^= 1;
		device_commit();
		
		htcleo_display_init();
		
		redraw_menu();
		selector_enable();
		return;
	}
	// BBT
	else if (!memcmp(command, "bbt_", 4 ))
	{	
		int j = atoi( command + 4 );
		device_info.fill_bbt_at_start = j;
		device_commit();
		change_menu_item(&sett_menu,
						(j ? "   FILL BBT @ STARTUP" : "   SKIP FILLING BBT @ STARTUP"),
						(j ? "   SKIP FILLING BBT @ STARTUP" : "   FILL BBT @ STARTUP"),
						(j ? "bbt_0" : "bbt_1"));
		redraw_menu();
		selector_enable();
		return;
	}
	// SET DEFAULT CPU FREQ
	else if (!memcmp(command, "freq_set", strlen(command) ))
	{
		shared_menu.top_offset		= 0;
		shared_menu.bottom_offset	= 0;
		shared_menu.selectedi		= device_info.cpu_freq;
		shared_menu.maxarl			= 0;
		shared_menu.goback			= 0;
		strcpy( shared_menu.MenuId, "DEFAULT CPU FREQUENCY" );
		strcpy( shared_menu.backCommand, "goto_sett" );
										
		add_menu_item(&shared_menu, "   768 MHZ", "freq_0", 0);
		add_menu_item(&shared_menu, "   998 MHZ", "freq_1", 1);
		
		active_menu = &shared_menu;
		redraw_menu();
		selector_enable();
		return;
	}
	// SET DEFAULT KERNEL (use shared_menu for confirmation dialog)
	else if (!memcmp(command, "kernel_set", strlen(command)))
	{
		if ( device_boot_ptn_num() ){
			shared_menu.top_offset		= 0;
			shared_menu.bottom_offset	= 0;
			shared_menu.selectedi		= device_info.boot_sel;
			shared_menu.maxarl			= 0;
			shared_menu.goback			= 0;
			strcpy( shared_menu.MenuId, "DEFAULT KERNEL" );
			strcpy( shared_menu.backCommand, "goto_sett" );

			int i=0;
			char menuTitle[64];
			char menuCommand[64];
			add_menu_item(&shared_menu, "   KERNEL @  boot", "kernel_0", i++);
			unsigned index = 1;
			while (index < 8) {
				if (device_partition_exist(supported_boot_partitions[index].name)){
					sprintf( menuTitle, "   KERNEL @ %s",
							supported_boot_partitions[index].name);
					sprintf( menuCommand, "kernel_%i", index);
					add_menu_item(&shared_menu, menuTitle, menuCommand, i++);
				}
				index++;
			}
			active_menu = &shared_menu;
			redraw_menu();
			selector_enable();
		}
		return;
	}
	// SET DEFAULT SCREEN BRIGHTNESS (use shared_menu for confirmation dialog)
	else if (!memcmp(command, "screenb_set", strlen(command)))
	{
		int i = 4;
		if (device_info.panel_brightness)
			i = device_info.panel_brightness - 1;
		shared_menu.top_offset		= 0;
		shared_menu.bottom_offset	= 0;
		shared_menu.selectedi		= i;
		shared_menu.maxarl			= 0;
		shared_menu.goback			= 0;
		strcpy( shared_menu.MenuId, "DEFAULT SCREEN BRIGHTNESS LEVEL" );
		strcpy( shared_menu.backCommand, "goto_sett" );

		add_menu_item(&shared_menu, "   ||", "screenl_1", 0);
		add_menu_item(&shared_menu, "   ||||", "screenl_2", 1);
		add_menu_item(&shared_menu, "   ||||||", "screenl_3", 2);
		add_menu_item(&shared_menu, "   ||||||||", "screenl_4", 3);
		add_menu_item(&shared_menu, "   ||||||||||", "screenl_5", 4);
		add_menu_item(&shared_menu, "   ||||||||||||", "screenl_6", 5);
		add_menu_item(&shared_menu, "   ||||||||||||||", "screenl_7", 6);
		add_menu_item(&shared_menu, "   ||||||||||||||||", "screenl_8", 7);
		add_menu_item(&shared_menu, "   ||||||||||||||||||", "screenl_9", 8);
		
		active_menu = &shared_menu;
		redraw_menu();
		selector_enable();
		return;
	}
	// SET DEFAULT UDC SETTINGS (use shared_menu for confirmation dialog)
	else if (!memcmp(command, "udc_set", strlen(command)))
	{
		int i = 0;
		if (device_info.udc)
			i = device_info.udc;
		shared_menu.top_offset		= 0;
		shared_menu.bottom_offset	= 0;
		shared_menu.selectedi		= i;
		shared_menu.maxarl			= 0;
		shared_menu.goback			= 0;
		strcpy( shared_menu.MenuId, "{  VENDOR_ID, PRODUCT_ID, VERSION_ID}" );
		strcpy( shared_menu.backCommand, "goto_sett" );
										
		add_menu_item(&shared_menu, "   {     0x18D1,     0xD00D,     0x0100}", "udc_0", 0);
		add_menu_item(&shared_menu, "   {     0x18D1,     0x0D02,     0x0001}", "udc_1", 1);
		add_menu_item(&shared_menu, "   {     0x0BB4,     0x0C02,     0x0100}", "udc_2", 2);
		add_menu_item(&shared_menu, "   {     0X05C6,     0x9026,     0x0100}", "udc_3", 3);

		active_menu = &shared_menu;
		redraw_menu();
		selector_enable();
		return;
	}
	// SET DEFAULT OFF-MODE CHARGING SETTINGS (use shared_menu for confirmation dialog)
	else if (!memcmp(command, "charge_set", strlen(command)))
	{
		shared_menu.top_offset		= 0;
		shared_menu.bottom_offset	= 0;
		shared_menu.selectedi		= device_info.use_inbuilt_charging;
		shared_menu.maxarl			= 0;
		shared_menu.goback			= 0;
		strcpy( shared_menu.MenuId, "DEFAULT OFF-MODE CHARGING" );
		strcpy( shared_menu.backCommand, "goto_sett" );
										
		add_menu_item(&shared_menu, "   USE RECOVERY'S OFF-MODE CHARGING", "charge_0", 0);
		add_menu_item(&shared_menu, "   USE INBUILT OFF-MODE CHARGING", "charge_1", 1);
		
		active_menu = &shared_menu;
		redraw_menu();
		selector_enable();
		return;
	}
	// SET DEFAULT CHARGING VOLTAGE THRESHOLD (use shared_menu for confirmation dialog)
	else if (!memcmp(command, "threshold_set", strlen(command)))
	{
		shared_menu.top_offset		= 0;
		shared_menu.bottom_offset	= 0;
		shared_menu.selectedi		= device_info.chg_threshold;
		shared_menu.maxarl			= 0;
		shared_menu.goback			= 0;
		strcpy( shared_menu.MenuId, "DEFAULT CHARGING VOLTAGE THRESHOLD" );
		strcpy( shared_menu.backCommand, "goto_sett" );
										
		add_menu_item(&shared_menu, "   3800 mV", "threshold_0", 0);
		add_menu_item(&shared_menu, "   3900 mV", "threshold_1", 1);
		add_menu_item(&shared_menu, "   4000 mV", "threshold_2", 2);
		add_menu_item(&shared_menu, "   4100 mV", "threshold_3", 3);
		add_menu_item(&shared_menu, "   4200 mV", "threshold_4", 4);
		
		active_menu = &shared_menu;
		redraw_menu();
		selector_enable();
		return;
	}
	// RESET SETTINGS (use shared_menu for confirmation dialog)
	else if (!memcmp(command,"reset_devinfo", strlen(command)))
	{
		shared_menu.top_offset		= 0;
		shared_menu.bottom_offset	= 0;
		shared_menu.selectedi		= 1;// CANCEL by default
		shared_menu.maxarl			= 0;
		shared_menu.goback			= 0;
		strcpy( shared_menu.MenuId, "RESET SETTINGS" );
		strcpy( shared_menu.backCommand, "goto_sett" );

		add_menu_item(&shared_menu, "   APPLY"	, "rset_1", 0);
		add_menu_item(&shared_menu, "   CANCEL"	, "rset_0", 1);

		active_menu = &shared_menu;
		redraw_menu();
		selector_enable();
		return;		
	}
 	// Return to MAIN MENU
	else if (!memcmp(command,"goto_main", strlen(command)))
	{
		active_menu = &main_menu;
		redraw_menu();
		selector_enable();
		return;
    }
	// ERROR
	else
	{
		subCommand = NULL;
		redraw_menu();
		printf("   HBOOT BUG: Somehow fell through sett_menu_command()\n");
		selector_enable();
		return;
	}
}

// INFO MENU
void eval_about_menu(char *command)
{
	// PRINT PARTITION TABLE
	if (!memcmp(command,"prnt_stat", strlen(command)))
	{
		redraw_menu();
		device_list();
		selector_enable();
		return;
	}
	// PRINT NAND INFO
	else if (!memcmp(command,"prnt_nand", strlen(command)))
	{
		redraw_menu();
		prnt_nand_stat();
		selector_enable();
		return;
	}
	// PRINT BAD BLOCK TABLE
	else if (!memcmp(command,"prnt_bblocks", strlen(command)))
	{
        redraw_menu();
		if( flash_bad_blocks < 0 ){
			printf("   BAD BLOCK TABLE was not created!\n");
		}else if( flash_bad_blocks > 0 ){
			printf("    ____________________________________________________ \n\
				   |                   BAD BLOCK TABLE                  |\n\
				   |____________________________________________________|\n");
			printf("   |       |   HOST    |  POSITION  | POSITION |        |\n\
				   | BLOCK | PARTITION | from START | from END | STATUS |\n");
			printf("   |=======|===========|============|==========|========|\n");
			for ( int j = 0; j < block_tbl.count; j++ )
			{
	      			printf( "   | %5d | %9s | %10d | %8d | %6s |\n",
	      				block_tbl.blocks[j].pos,
	      				block_tbl.blocks[j].partition,
	      				block_tbl.blocks[j].pos_from_pstart,
	      				block_tbl.blocks[j].pos_from_pend,
					block_tbl.blocks[j].is_marked==1 ? "MARKED" : "ERROR");
			}
			printf("   |_______|___________|____________|__________|________|   ");
		}else{
			printf("   No MARKED bad blocks detected!\n");
		}
		selector_enable();
		return;
    }
   	// CREDITS
	else if (!memcmp(command,"goto_credits", strlen(command)))
	{
		redraw_menu();
		cmd_oem_credits();
		selector_enable();
		return;
    }
	// HELP
	else if (!memcmp(command,"goto_help", strlen(command)))
	{
		shared_menu.top_offset		= 0;
		shared_menu.bottom_offset	= 0;
		shared_menu.selectedi		= 0;
		shared_menu.maxarl			= 0;
		shared_menu.goback			= 0;
		strcpy( shared_menu.MenuId, "HELP" );
		strcpy( shared_menu.backCommand, "goto_about" );
		add_menu_item(&shared_menu, ">> HELP (fastboot oem help)", "goto_about", 0);
		
		fbcon_clear_region(0, 45 , (inverted ? 0xffff : 0x0000));
		fbcon_set_y_cord(0);fbcon_set_x_cord(0);
		
		active_menu = &shared_menu;
		_dputs(active_menu->item[0].mTitle);
		_dputs("\n");
		oem_help();
		selector_enable();
		return;
    }
 	// Return to MAIN MENU
	else if (!memcmp(command,"goto_main", strlen(command)))
	{
		eval_sett_menu(command);
		return;
    }
}

// SHARED MENU
void eval_shared_menu(char *command)
{
	// Return to INFO MENU
	if (!memcmp(command,"goto_about", strlen(command)))
	{
		active_menu = &about_menu;
		redraw_menu();
		selector_enable();
		return;
    }
	// Option to show the partition table as it would be with the new partition order
	else if (!memcmp(command,"prnt_mirror_info", strlen(command)))
	{
		printf("    ____________________________________________________ \n\
			   |                  PARTITION  TABLE                  |\n\
			   |____________________________________________________|\n\
			   | MTDBLOCK# |   NAME   | AUTO-SIZE |  BLOCKS  |  MB  |\n");
		printf("   |===========|==========|===========|==========|======|\n");
		int ordercount=1;
		for ( unsigned i = 1; i < MAX_NUM_PART+1; i++ )	{
			for ( unsigned j = 0; j < MAX_NUM_PART; j++ ) {
				if( ((int)i==mirror_info.partition[j].order)
				&& (strlen( mirror_info.partition[j].name ) != 0) )
				{
					if( memcmp(mirror_info.partition[j].name, "lk", strlen("lk")) ) {
						printf( "   | mtdblock%2i| %8s |     %i     |   %4i   | %3i  |\n",
								ordercount++, mirror_info.partition[j].name,
								mirror_info.partition[j].asize, mirror_info.partition[j].size,
								round((float)(mirror_info.partition[j].size)/(float)(get_blk_per_mb())) );
					}
				}
			}
		}
		printf("   |___________|__________|___________|__________|______|\n");		
		return;
	}
	else if (!memcmp(command, "kernel_", 7 ))
	{
		redraw_menu();
		int j = atoi( command + 7 );
		device_info.boot_sel = j;
		device_commit();
		printf("   Default KERNEL selection has been successfully saved.\n");
		mdelay(2000);
		active_menu = &sett_menu;
		redraw_menu();
		selector_enable();
		return;
	}
	else if (!memcmp(command, "screenl_", 8 ))
	{
		redraw_menu();
		int j = atoi( command + 8 );
		device_info.panel_brightness = j;
		device_commit();
		printf("   Setting screen brightness level. Please wait...\n");
		selector_disable();
		htcleo_panel_set_brightness(j);
		redraw_menu();
		selector_enable();
		return;
	}
	else if (!memcmp(command, "udc_", 4 ))
	{
		redraw_menu();
		int j = atoi( command + 4 );
		device_info.udc = j;
		device_commit();
		active_menu = &main_menu;
		redraw_menu();
		printf("   Udc settings changed! Device will reboot in 2s...\n");
		mdelay(2000);
		target_reboot(FASTBOOT_MODE);
		return;
	}
	else if (!memcmp(command, "charge_", 7 ))
	{
		redraw_menu();
		int j = atoi( command + 7 );
		device_info.use_inbuilt_charging = j;
		device_commit();
		printf("   Off-mode charging selection has been successfully saved.\n");
		mdelay(2000);
		active_menu = &sett_menu;
		redraw_menu();
		selector_enable();
		return;
	}
	else if (!memcmp(command, "threshold_", 10 ))
	{
		redraw_menu();
		int j = atoi( command + 10 );
		device_info.chg_threshold = j;
		device_commit();
		printf("   Charging voltage threshold has been successfully saved.\n");
		mdelay(2000);
		active_menu = &sett_menu;
		redraw_menu();
		selector_enable();
		return;
	}
	else if (!memcmp(command, "freq_", 5 ))
	{
		int j = atoi( command + 5 );
		device_info.cpu_freq = j;
		device_commit();
		
		active_menu = &main_menu;
		redraw_menu();
		printf("   Cpu freq changed! Device will reboot in 2s...\n");
		mdelay(2000);
		target_reboot(FASTBOOT_MODE);
		return;
	}
	else if (!memcmp(command,"goto_sett", strlen(command)))
	{
		eval_main_menu("goto_sett");
		return;
	}
	else if ( !memcmp( command, "frmt_", 5 ) )
	{	
		int j = atoi( command + 5 );
		redraw_menu();
		if(j) {
			printf( "   Initializing flash format..." );
			
			struct ptable *ptable;
			ptable = flash_get_ptable();
			if(ptable == NULL) 
			{
				printf( "   ERROR: PTABLE not found!!!\n");
				return;
			}

			// Format all partitions except for recovery and lk
			time_t ct = current_time();
			for ( int i = 0; i < ptable_size(ptable); i++ )
			{
				if( (memcmp(ptable_get(ptable, i)->name,"recovery", strlen(ptable_get(ptable, i)->name)))
				&& (memcmp(ptable_get(ptable, i)->name,"lk", strlen(ptable_get(ptable, i)->name))) )
				{
					printf( "\n   Formatting %s...", ptable_get(ptable, i)->name );
					flash_erase(ptable_get(ptable, i));
				}
			}
			ct = current_time() - ct;
			printf("\n   Format completed in %i msec!\n", ct);
		} else {
			active_menu = &sett_menu;
			redraw_menu();
		}
		selector_enable();
		return;
	}
	else if ( !memcmp( command, "rset_", 5 ) )
	{	
		int j = atoi( command + 5 );
		redraw_menu();
		if(j) {		
			struct ptentry *ptn;
			struct ptable *ptable;
					
			ptable = flash_get_devinfo();
			if (ptable == NULL)
			{
				printf( "   ERROR: DEVINFO not found!!!\n");
				return;
			}
				
			ptn = ptable_find(ptable, "devinf");
			if (ptn == NULL)
			{
				printf( "   ERROR: No DEVINFO section!!!\n");
				return;
			}
			
			printf("   Wiping devinfo...\n");
			flash_erase(ptn);
					
			printf("   Settings were reset !\n   Device will reboot in 3s to load default settings...");
			mdelay(3000);
			target_reboot(FASTBOOT_MODE);
		} else {
			active_menu = &sett_menu;
			redraw_menu();
		}
		selector_enable();
		return;
	}
	// ERROR
	else
	{
		redraw_menu();
		printf("   HBOOT BUG: Somehow fell through eval_shared_menu()\n");
		selector_enable();
		return;
	}
}

// REPARTITION
void eval_rept_menu(char *command)
{
	char* subCommand = command + 5;
	// REPARTITION MENU
	if ( active_menu == &rept_menu )
	{
		if (!memcmp(command,"goto_sett", strlen(command)))
		{
			eval_main_menu(command);
			return;
		}
		// PRINT PARTITION TABLE
		else if (!memcmp(command,"prnt_stat", strlen(command)))
		{
			eval_about_menu(command);
			return;
		}
		else if ( !memcmp( subCommand, "commit", strlen( subCommand ) ) )
		{
			rept_sub_menu.top_offset	= 0;
			rept_sub_menu.bottom_offset	= 0;
			rept_sub_menu.selectedi		= 0;// APPLY by default
			rept_sub_menu.maxarl		= 0;
			rept_sub_menu.goback		= 0;

			strcpy( rept_sub_menu.MenuId, "COMMIT CHANGES" );
			strcpy( rept_sub_menu.backCommand, "goto_rept" );

			add_menu_item(&rept_sub_menu, "   APPLY"	, "rept_write", 0);
			add_menu_item(&rept_sub_menu, "   CANCEL"	, "goto_rept", 1);

			active_menu = &rept_sub_menu;

			redraw_menu();
			selector_enable();
			printf(" \n\n\n");
			device_list();
			return;
		}
		else
		{
			if(strcmp(subCommand, "lk"))
			{
				rept_sub_menu.top_offset    = 0;
				rept_sub_menu.bottom_offset = 0;
				rept_sub_menu.selectedi     = 0;
				rept_sub_menu.maxarl		= 0;
				rept_sub_menu.goback		= 0;
		
				strcpy( rept_sub_menu.data, subCommand );
				strcpy( rept_sub_menu.backCommand, "goto_rept" );
		
				if(!device_info.partition[device_partition_order(rept_sub_menu.data)-1].asize) // if selected partition has a fixed size
				{
					sprintf( rept_sub_menu.MenuId, "%s = %d MB", rept_sub_menu.data, (int) round((float)(device_partition_size(rept_sub_menu.data)) / (float)(get_blk_per_mb())) );
							
					/* koko : Added more options +25,+5,-5,-25 */
					add_menu_item(&rept_sub_menu, "   +25"			, "rept_add_25", 0);
					add_menu_item(&rept_sub_menu, "   +10"			, "rept_add_10", 1);
					add_menu_item(&rept_sub_menu, "   +5"			, "rept_add_5" , 2);
					add_menu_item(&rept_sub_menu, "   +1"			, "rept_add_1" , 3);
					add_menu_item(&rept_sub_menu, "   -1"			, "rept_rem_1" , 4);
					add_menu_item(&rept_sub_menu, "   -5"			, "rept_rem_5" , 5);
					add_menu_item(&rept_sub_menu, "   -10"			, "rept_rem_10", 6);
					add_menu_item(&rept_sub_menu, "   -25"			, "rept_rem_25", 7);
				}
				else 	// if selected partition is auto-size
				{		// give user the option to make it fixed-size
					sprintf( rept_sub_menu.MenuId, "%s partition is auto-size (%d MB)", rept_sub_menu.data , (int) (device_partition_size(rept_sub_menu.data) / get_blk_per_mb()) );
					char cancelautosize[32];
					char cancelautosizecmd[32];
					sprintf( cancelautosize, "   Convert partition to fixed-size (%d MB)", (int) (device_partition_size(rept_sub_menu.data) / get_blk_per_mb()) );
					sprintf( cancelautosizecmd, "rept_set_%d", (int) (device_partition_size(rept_sub_menu.data) / get_blk_per_mb()) );
					add_menu_item(&rept_sub_menu, cancelautosize		, cancelautosizecmd, 0);
				}
		
				active_menu = &rept_sub_menu;
				redraw_menu();
				selector_enable();
				return;
			}
		}
	}
	// REPARTITION SUBMENU
	else if ( active_menu == &rept_sub_menu )
	{
		if (!memcmp(command,"goto_rept", strlen(command)))
		{
			eval_sett_menu(command);
			return;
		}
		else if ( !memcmp( subCommand, "set_", 4 ) )
		{	// option to make auto-sized partition -> fixed size
			int add = atoi( subCommand + 4 );// MB
			device_info.partition[device_partition_order(rept_sub_menu.data)-1].asize = 0;
			device_resize_ex( rept_sub_menu.data, (add*get_blk_per_mb()), true );
			sprintf( rept_sub_menu.MenuId, "%s = %d MB", rept_sub_menu.data, add );
			mdelay(500);
			active_menu = &rept_menu;
			redraw_menu();
			selector_enable();
			return;
		}
		else if ( !memcmp( subCommand, "add_", 4 ) )
		{
			int add = atoi( subCommand + 4 );// MB
			int size = (int) device_partition_size( rept_sub_menu.data ) + (add*get_blk_per_mb());
			device_resize_ex( rept_sub_menu.data, size, true );
			sprintf( rept_sub_menu.MenuId, "%s = %d MB", rept_sub_menu.data, size/get_blk_per_mb() );
			redraw_menu();
			selector_enable();
			return;
		}
		else if ( !memcmp( subCommand, "rem_", 4 ) )
		{
			int rem = atoi( subCommand + 4 );// MB
			int size = (int) device_partition_size( rept_sub_menu.data ) - (rem*get_blk_per_mb());
			/* koko : User can now set a partition as variable if one doesn't already exist */

			if(!device_variable_exist())// Variable partition doesn't exist
			{							// Can set any partition as variable
				if ( size > 0 )
				{
					device_resize_ex( rept_sub_menu.data, size, true );
					sprintf( rept_sub_menu.MenuId, "%s = %d MB", rept_sub_menu.data, size/get_blk_per_mb() );
				} else
				if ( size == 0 )	// 0 is accepted
				{
					device_info.partition[device_partition_order(rept_sub_menu.data)-1].asize = 1;
					device_resize_ex( rept_sub_menu.data, size, true );
					sprintf( rept_sub_menu.MenuId, "%s partition is auto-size (%d MB)", rept_sub_menu.data , size/get_blk_per_mb() );
					mdelay(500);
					active_menu = &rept_menu;
				}
			}
			else 				// Variable partition already exists
			{					// Can not set another partition as variable
				if ( size > 0 )	// 0 is not accepted
				{
					device_resize_ex( rept_sub_menu.data, size, true );
					sprintf( rept_sub_menu.MenuId, "%s = %d MB", rept_sub_menu.data, size/get_blk_per_mb() );
				}
			}
			redraw_menu();
			selector_enable();
			return;
		}
		else if ( !memcmp( subCommand, "write", strlen( subCommand ) ) )
		{
			// Apply changes
			device_resize_asize();
			device_commit();

			/* Load new PTABLE layout without rebooting */
			printf("\n       New PTABLE layout has been successfully loaded");
			htcleo_ptable_re_init();
			mdelay(2000);
			active_menu = &sett_menu;
			redraw_menu();
			selector_enable();
			return;
		}
	}
	// ERROR
	else
	{
		subCommand = NULL;
		redraw_menu();
		printf("   HBOOT BUG: Somehow fell through eval_rept_menu()\n");
		selector_enable();
		return;
	}
}

// REARRANGE
void eval_rear_menu(char *command)
{
	char* subCommand = command + 5;
	// REARRANGE MENU
	if ( active_menu == &rear_menu )
	{
		if (!memcmp(command,"goto_sett", strlen(command)))
		{
			eval_main_menu(command);
			return;
		}
		// Option to show the partition table as it would be with the new partition order
		else if (!memcmp(command,"prnt_mirror_info", strlen(command)))
		{
			redraw_menu();
			eval_shared_menu(command);
			selector_enable();
			return;
		}
		else if ( !memcmp( subCommand, "commit", strlen( subCommand ) ) )
		{
			rear_sub_menu.top_offset	= 0;
			rear_sub_menu.bottom_offset	= 0;
			rear_sub_menu.selectedi		= 0;	// APPLY by default
			rear_sub_menu.maxarl		= 0;
			rear_sub_menu.goback		= 0;

			strcpy( rear_sub_menu.MenuId, "COMMIT CHANGES" );
			strcpy( rear_sub_menu.backCommand, "goto_rear" );

			add_menu_item(&rear_sub_menu, "   APPLY"	, "rear_write", 0);
			add_menu_item(&rear_sub_menu, "   CANCEL"	, "goto_rear" , 1);

			active_menu = &rear_sub_menu;
			redraw_menu();
			selector_enable();
			printf(" \n\n\n");
			eval_shared_menu("prnt_mirror_info");
			return;
		}
		else
		{
			// Don't move lk or recovery partition
			if(strcmp(subCommand, "recovery") && strcmp(subCommand, "lk"))
			{
				strcpy( rear_sub_menu.data, subCommand );
				sprintf(rear_sub_menu.MenuId,
						"%s as partition #%d",
						rear_sub_menu.data,
						(int)(mirror_partition_order(rear_sub_menu.data)));
				strcpy( rear_sub_menu.backCommand, "goto_rear" );
				/*
				 * for every partition listed, the scale of order numbers is
				 * [0] to [number-of-all-partitions]
				 * 0 can be used in order to remove a partition
				 * Because we don't need to (must mot) remove the default
				 * partitions recovery,misc,boot,cache
				 * 0 is excluded from their scale
				*/
				int orderstart=0;
				// fix menu selection if recovery is not the 1st partition
				int menuselectfix=1;
				if((!strcmp(rear_sub_menu.data, "misc"))
				|| (!strcmp(rear_sub_menu.data, "boot"))
				|| (!strcmp(rear_sub_menu.data+1, "boot"))
				|| (!strcmp(rear_sub_menu.data, "cache")))
				{
					if(device_partition_order( "recovery" )==2)	{
						orderstart=3;menuselectfix=0;
					} else {
						orderstart=2;
					}
				}
				//Lets give the option to remove system,userdata -> orderstart=0
				if( (!strcmp(rear_sub_menu.data, "userdata")) || (!strcmp(rear_sub_menu.data, "system")) ) {
					if(device_partition_order( "recovery" )==2)	{
						orderstart=0;menuselectfix=2;
					} else {
						orderstart=0;
					}
				}
				if( (device_partition_order(rear_sub_menu.data))<(device_partition_order("recovery")) )	{
					menuselectfix=0;
				}
							
				rear_sub_menu.top_offset    = 0;
				rear_sub_menu.bottom_offset = 0;
				rear_sub_menu.selectedi     = (int)(mirror_partition_order(rear_sub_menu.data)
													- orderstart
													- menuselectfix);
				rear_sub_menu.maxarl		= 0;
				rear_sub_menu.goback		= 0;

				char ordernumber[32];
				char ordercmd[32];
				int j=0;
				for ( int i = orderstart; i < (active_menu->maxarl - 1); i++ ) {
					// The order of lk and recovery partitions can not be available
					if( i!=device_partition_order( "recovery" ) && i!=device_partition_order( "lk" ) ) {
						sprintf( ordernumber, "   %d", i );
						sprintf( ordercmd, "rear_set_%d", i );
						add_menu_item(&rear_sub_menu, ordernumber			, ordercmd, j++);
					}
				}

				active_menu = &rear_sub_menu;
				redraw_menu();
				selector_enable();
				return;
			}
		}
	}
	// REARRANGE SUB MENU
	else if ( active_menu == &rear_sub_menu )
	{
		if (!memcmp(command,"goto_rear", strlen(command)))
		{
			eval_sett_menu(command);
			return;
		}
		else if ( !memcmp( subCommand, "set_", 4 ) )
		{
			int neworder = atoi( subCommand + 4 );
			int oldorder = 0;
			for ( unsigned i = 0; i < MAX_NUM_PART; i++ ) {
				if ( !memcmp( mirror_info.partition[i].name, rear_sub_menu.data, strlen( rear_sub_menu.data ) ) ) {
					// save the initial position of the partition we selected to move
					// cause after changing the partition's position the initial one is available
					oldorder = mirror_info.partition[i].order;
					// if we set 0 for a partition's order, then that partition will be removed
					if(neworder==0)	{
						// if the partition was the first one,
						// then we use the freed space by expanding the next one
						// otherwise we use the freed space by expanding the previous one
						mirror_info.partition[(i==1 ? (i+1) : (i-1))].size += mirror_info.partition[i].size;
						// 'remove' the partition from the list and
						// fix the order of the following partitions
						strcpy( mirror_info.partition[i].name, "" );
						for ( unsigned j = 0; j < MAX_NUM_PART; j++ ) {
							if(mirror_info.partition[j].order > oldorder) {
								mirror_info.partition[j].order--;
							}
						}
					} else {
						for ( unsigned j = 0; j < MAX_NUM_PART; j++ ) {
							// find the partition that is in the place
							// where we will move the selected partition
							if(mirror_info.partition[j].order == neworder) {
								// place that partition to the empty initial
								// position of the selected partition
								mirror_info.partition[j].order = oldorder;
								break;
							}
						}
						// change the selected partition's position
						mirror_info.partition[i].order = neworder;
					}
					//break;
				}
			}
			
			mdelay(500);
				
			rear_menu.top_offset	= 0;
			rear_menu.bottom_offset	= 0;
			rear_menu.maxarl		= 0;
			rear_menu.goback		= 0;

			char cmd[64];
			char partname[64];
			int k=0;
			for ( unsigned i = 1; i < MAX_NUM_PART+1; i++ )	{
				for ( unsigned j = 0; j < MAX_NUM_PART; j++ ) {
					if( ((int)i==mirror_info.partition[j].order)
					&& (strlen( mirror_info.partition[j].name ) != 0) )
					{
						strcpy( cmd, "rear_" );
						strcat( cmd, mirror_info.partition[j].name );
						sprintf( partname, "   %s", mirror_info.partition[j].name);
						add_menu_item( &rear_menu, partname, cmd , k++ );
					}
				}
			}
			add_menu_item( &rear_menu, "   PRINT PARTITION TABLE", "prnt_mirror_info" , k++ );
			add_menu_item( &rear_menu, "   COMMIT CHANGES", "rear_commit" , k );

			active_menu = &rear_menu;
			redraw_menu();
			selector_enable();
			return;
		}
		else if ( !memcmp( subCommand, "write", strlen( subCommand ) ) )
		{
			// Apply changes
			char name_and_size[64];
			device_clear();
			for ( unsigned i = 1; i < MAX_NUM_PART+1; i++ )	{
				for ( unsigned j = 0; j < MAX_NUM_PART; j++ ) {
					if( ((int)i==mirror_info.partition[j].order)
					&& (strlen( mirror_info.partition[j].name ) != 0) )
					{
						// if partition is auto-size
						if(mirror_info.partition[j].asize) {
							sprintf( name_and_size, "%s:%d", mirror_info.partition[j].name, 0 );
							device_add(name_and_size);
						} else {
						// if partition has a fixed size
							sprintf( name_and_size, "%s:%d:b", mirror_info.partition[j].name, mirror_info.partition[j].size );
							device_add(name_and_size);
						}
					}
				}
			}
			device_resize_asize();
			device_commit();

			/* Load new PTABLE layout without rebooting */
			printf("\n       New PTABLE layout has been successfully loaded");
			htcleo_ptable_re_init();
			mdelay(2000);
			active_menu = &sett_menu;
			redraw_menu();
			selector_enable();
			return;
		}
	}
	// ERROR
	else
	{
		subCommand = NULL;
		redraw_menu();
		printf("   HBOOT BUG: Somehow fell through eval_rear_menu()\n");
		selector_enable();
		return;
	}
}

void eval_command(void)
{
	int active_menu_id = active_menu->id;;

	if (active_menu->goback) {
		active_menu->goback = 0;
		strcpy(command_to_execute, active_menu->backCommand);
		if ( strlen( command_to_execute ) == 0 )
			return;
	} else {
		strcpy(command_to_execute, active_menu->item[active_menu->selectedi].command);
	}
		/* Check which menu is active	 		and		call that menu's eval_cmd */
	/*!*/if(active_menu_id == MAIN_MENU_ID)				eval_main_menu(command_to_execute);
	else if(active_menu_id == SETT_MENU_ID)				eval_sett_menu(command_to_execute);
	else if(active_menu_id == REPT_MENU_ID)				eval_rept_menu(command_to_execute);
	else if(active_menu_id == REAR_MENU_ID)				eval_rear_menu(command_to_execute);
	else if(active_menu_id == SHARED_MENU_ID)			eval_shared_menu(command_to_execute);
	else if(active_menu_id == ABOUT_MENU_ID)			eval_about_menu(command_to_execute);
	else command_to_execute[0] = '\0';
}

void redraw_menu(void)
{
	fbcon_resetdisp();
	draw_clk_header();
	
	int current_offset = fbcon_get_y_cord();

	if (active_menu->top_offset < 1)
		active_menu->top_offset = current_offset;
	else
		fbcon_set_y_cord(active_menu->top_offset);

	if (active_menu->top_offset > 1 && active_menu->bottom_offset > 1)
		fbcon_clear_region( active_menu->top_offset,
							active_menu->bottom_offset,
							(inverted ? 0xffff : 0x0000));
		
	for (uint8_t i=0;; i++) {
		if ((strlen(active_menu->item[i].mTitle) != 0)
		&& !(i >= active_menu->maxarl) )
		{
			fbcon_setfg((inverted ? 0x0000 : 0xffff));

			if(active_menu->item[i].x == 0)
				active_menu->item[i].x = fbcon_get_x_cord();
			if(active_menu->item[i].y == 0)
				active_menu->item[i].y = fbcon_get_y_cord();

			_dputs(active_menu->item[i].mTitle);
			_dputs("\n");
		} else {
			break;
		}
	}

	_dputs("\n");
		
	if (active_menu->bottom_offset < 1)
		active_menu->bottom_offset = fbcon_get_y_cord();
	else
		fbcon_set_y_cord(active_menu->bottom_offset);

	if (current_offset > fbcon_get_y_cord())
		fbcon_set_y_cord(current_offset);
}

static int menu_item_nav(unsigned key_code)
{
	thread_set_priority(HIGHEST_PRIORITY);
	
	if (didyouscroll())
		redraw_menu();
	
	int current_y_offset = fbcon_get_y_cord();
	int current_x_offset = fbcon_get_x_cord();
	
	selector_disable();
	
	switch (key_code) {
		case KEY_VOLUMEUP:
			if ((active_menu->selectedi) == 0)
				active_menu->selectedi=(active_menu->maxarl-1);
			else
				active_menu->selectedi--;
			break;
		case KEY_VOLUMEDOWN:
			if (active_menu->selectedi == (active_menu->maxarl-1))
				active_menu->selectedi=0;
			else
				active_menu->selectedi++;
			break;
	}
	
	selector_enable();
	
	fbcon_set_y_cord(current_y_offset);
	fbcon_set_x_cord(current_x_offset);
	
	thread_set_priority(DEFAULT_PRIORITY);
	return 0;
}

#if TIMER_HANDLES_AUTOBOOT_EVENT
struct timer autoboot;
int autoboot_timer_active;
static enum handler_return countdown_off_event(struct timer *timer, time_t now, void *arg)
{
	//If the user didn't select any option, restore default selection,
	//and boot up from the default kernel
	timer_cancel(timer);
	main_menu.selectedi = device_info.boot_sel;
	eval_command();

	return INT_NO_RESCHEDULE;
}
#endif

void eval_keydown(unsigned key_code)
{
	switch (key_code)
	{
		case KEY_VOLUMEUP: case KEY_VOLUMEDOWN:
			menu_item_nav(key_code);
#if TIMER_HANDLES_AUTOBOOT_EVENT
			// If in MULTIBOOT menu and the user pressed
			// any of the vol keys to change the selected
			// kernel, STOP the autoboot timer so that he
			// has the time to boot from his choice
			if(autoboot_timer_active) {
				enter_critical_section();
				timer_cancel(&autoboot);
				exit_critical_section();
				autoboot_timer_active = 0;
			}
#endif
			break;
		case KEY_SEND/*dial*/: case KEY_SOFT1/*windows*/:
			eval_command();
			break;
		case KEY_HOME:
			active_menu = &main_menu;
			redraw_menu();
			selector_enable();
			break;
		case KEY_BACK:
			active_menu->goback = 1;
			eval_command();
			break;
		case KEY_POWER/*hangup*/:
		case 0/*keyup*/:
		default:
			break;
	}
}

/*
 * ...WIP...
 * koko: Attempt to handle key press events with a timer, to get rid of key_listener thread
 *
 * INITIAL IMPLEMENTATION ISSUES :	1)Can't boot any kernel							[  !  ]
 *									2)Flashlight can't turn off						[  !  ]
 *									3)Fastboot doesn't work after pressing any key 	[  !  ]
 */
#if TIMER_HANDLES_KEY_PRESS_EVENT
struct timer key_listener_timer;
static int keydown(void *arg)
{
	eval_keydown((unsigned)arg);
	while (keys_get_state((unsigned)arg) != 0) thread_sleep(1);
	
	thread_exit(0);
	return 0;
}
static enum handler_return key_listener_tick(struct timer *timer, time_t now, void *arg)
{
	unsigned key_code = 0;
	for(int i=0; i<7; i++) {
		if (keys_get_state(keys[i]) != 0) {
			key_code = keys[i];
			keyp_thread = thread_create("keydown",
										&keydown,
										(void *)key_code,
										DEFAULT_PRIORITY,
										DEFAULT_STACK_SIZE);
			thread_resume(keyp_thread);					
			break;
		}
    }
	timer_set_oneshot(timer, 200, key_listener_tick, NULL);
	
	return INT_NO_RESCHEDULE;
}
#endif
int total_keys = sizeof(keys)/sizeof(uint16_t);
static int key_listener(void *arg)
{
	while (key_listen) {
        for(int i=0; i<total_keys; i++) {
			if (keys_get_state(keys[i]) != 0) {
				eval_keydown(keys[i]);
				while (keys_get_state(keys[i]) != 0) mdelay(1);
				break;
			}
        }
	}
	
	thread_exit(0);
	return 0;
}

/*
 * koko : Get rom name from kernel's cmdline arg
 *		  as long as it is passed through 'rel_path'
 *		  as 1st or 2nd arg
 */
char *get_rom_name_from_cmdline(struct ptentry *ptn)
{
	struct boot_img_hdr *hdr = (void*) buf;
	char *cmdline;
	char *tmp_buff;
	
	if(flash_read(ptn, 0, buf, page_size)) {
		return "ERROR";
	}
 	if(memcmp(hdr->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE)) {
		return "INVALID";
	}
	if (hdr->cmdline[0]) {
		cmdline = (char*) hdr->cmdline;
	} else {
		return "NAND";
	}
	
 	//In case the cmdline contains '\r' ,wtf?
	if (strpbrk(cmdline, "\r") != NULL){
		//cmdline = sreplace(cmdline, '\r', strlen(cmdline));
		return "CrInCMDLINE";
	}

	//Check if the cmdline has more then one args
	if (strpbrk(cmdline, " ") != NULL) {
		//check rel_path=$RomName as 1st arg
		tmp_buff = strtok( cmdline, " " );
		if (!memcmp(tmp_buff, "rel_path=", 9 )) {
			tmp_buff = strchr( tmp_buff, '=' );
			//final check for the lenght
			return (strlen(tmp_buff) > 43 ? "LONGNAME" : (tmp_buff+1));
		} else {
			//check rel_path=$RomName as 2nd arg
			tmp_buff = strtok( NULL, " " );
			if (!memcmp(tmp_buff, "rel_path=", 9 )) {
				tmp_buff = strchr( tmp_buff, '=' );
				//final check for the lenght
				return (strlen(tmp_buff) > 43 ? "LONGNAME" : (tmp_buff+1));
			}
		}
	} else {
		//only check rel_path=$RomName as 1st arg
		if (!memcmp(cmdline, "rel_path=", 9 )) {
			tmp_buff = strchr( cmdline, '=' );
			//final check for the lenght
			return (strlen(tmp_buff) > 43 ? "LONGNAME" : (tmp_buff+1));
		}
	}
		
	return "UNDEFINED";
}

void init_multiboot_menu()
{	
	int i=1;
	char menuTitle[64];
	char menuCommand[64];
	main_menu.top_offset    = 0;
	main_menu.bottom_offset = 0;
	main_menu.maxarl		= 0;
	main_menu.goback		= 0;
	main_menu.id 			= 1;
	strcpy( main_menu.MenuId, "MULTIBOOT MENU" );
	strcpy( main_menu.backCommand, "" );
	
	int index = 1;
	// Add any existing boot partitions with the rom's name (check get_rom_name_from_cmdline)
	struct ptable *ptable = flash_get_ptable();
	//THE PRIMARY BOOT
	sprintf(menuTitle, "   KERNEL @ boot [%s]",
					get_rom_name_from_cmdline(ptable_find(ptable, "boot")));
	add_menu_item(&main_menu, menuTitle, "boot_0", 0);
	//THE REST BOOT PARTITIONS
	for (index = 1; index < 8; index++) {
		for ( int y = ptable_size(ptable); y--; ) {
			if(!memcmp(ptable_get(ptable, y)->name, supported_boot_partitions[index].name,	strlen(ptable_get(ptable, y)->name))) {
				sprintf(menuTitle, "   KERNEL @ %s [%s]",
						ptable_get(ptable, y)->name,
						get_rom_name_from_cmdline(ptable_get(ptable, y)));
				sprintf( menuCommand, "boot_%i", index);
				add_menu_item(&main_menu, menuTitle, menuCommand, i++);
				break;
			}
		}
	}
	/* 
	// Add any existing boot partitions without the rom's name
	//THE PRIMARY BOOT
	add_menu_item(&main_menu, "   KERNEL @ boot"	, "boot_0"	, i++);
	//THE REST BOOT PARTITIONS
	while (index++ < 8) {
		if (device_partition_exist(supported_boot_partitions[index].name)){
			sprintf( menuTitle, "   KERNEL @ %s",
					supported_boot_partitions[index].name);
			sprintf( menuCommand, "boot_%i", index);
			add_menu_item(&main_menu, menuTitle	, menuCommand	, i++);
		}
	}
	 */
	//cLK BOOT
	add_menu_item(&main_menu, "   REBOOT cLK"	, "acpu_bgwp"	, i++);
	main_menu.selectedi = device_info.boot_sel;
	
	active_menu = &main_menu;
	redraw_menu();
	printf("\n Select from which boot partition LK will load the kernel\
			\n\n %.s will load in 10s if NO interaction",
			active_menu->item[active_menu->selectedi].mTitle + 3);
	selector_enable();
	// Turn keys backlight OFF
	htcleo_key_bkl_pwr(0);
	
#if TIMER_HANDLES_KEY_PRESS_EVENT
	enter_critical_section();
	timer_initialize(&key_listener_timer);
	timer_set_oneshot(&key_listener_timer, 0, key_listener_tick, NULL);
	exit_critical_section();
#else
	keyp_thread = thread_create("start_keylistener",
								&key_listener,
								NULL,
								DEFAULT_PRIORITY,
								DEFAULT_STACK_SIZE);
	thread_resume(keyp_thread);
#endif

#if TIMER_HANDLES_AUTOBOOT_EVENT
	enter_critical_section();
	timer_initialize(&autoboot);
	timer_set_oneshot(&autoboot, 10000, countdown_off_event, NULL);
	exit_critical_section();
	autoboot_timer_active = 1;
#else
	// Set the countdown to 10 sec
	int countdown = 10000;
	while(countdown){
		// If the user pressed any of the vol keys to
		// change the default selected kernel,
		// STOP the autoboot timer so that he
		// has the time to boot from his choice
		if(keys_get_state(KEY_VOLUMEUP) != 0
		|| keys_get_state(KEY_VOLUMEDOWN) != 0)
		break;
		// Otherwise continue the countdown
		mdelay(100);
		countdown -= 100;
	}
	//If the user didn't select any option, and the
	//countdown finished restore default selection,
	//and boot up from the default kernel
	if(countdown==0) {
		main_menu.selectedi = device_info.boot_sel;
		eval_command();
	}
#endif
}

void init_menu()
{	
	/*****************MAIN MENU*****************/
	int i=1;
	int j=2;
	char menuTitle[64];
	char menuCommand[64];	
	main_menu.top_offset    = 0;
	main_menu.bottom_offset = 0;
	main_menu.maxarl		= 0;
	main_menu.goback		= 0;
	main_menu.id 			= MAIN_MENU_ID;
	strcpy( main_menu.MenuId, "MAIN MENU" );
	strcpy( main_menu.backCommand, "" );
	
	// Add any existing boot partitions with the rom's name (check get_rom_name_from_cmdline)
	int index;
	struct ptable *ptable = flash_get_ptable();
	//THE PRIMARY BOOT
	sprintf(menuTitle, "   KERNEL @ boot [%s]",
					get_rom_name_from_cmdline(ptable_find(ptable, "boot")));
	add_menu_item(&main_menu, menuTitle, "boot_0", 0);
	//THE REST BOOT PARTITIONS
	for (index = 1; index < 8; index++) {
		for ( int y = ptable_size(ptable); y--; ) {
			if(!memcmp(ptable_get(ptable, y)->name, supported_boot_partitions[index].name,	strlen(ptable_get(ptable, y)->name))) {
				sprintf(menuTitle, "   KERNEL @ %s [%s]",
						ptable_get(ptable, y)->name,
						get_rom_name_from_cmdline(ptable_get(ptable, y)));
				sprintf( menuCommand, "boot_%i", index);
				add_menu_item(&main_menu, menuTitle, menuCommand, i++);
				j++;//increase so that FLASHLIGHT will be selected by default
				break;
			}
		}
	}
	/* 
	// Add any existing boot partitions without the rom's name
	//THE PRIMARY BOOT
	add_menu_item(&main_menu, "   KERNEL @ boot", "boot_0", i++);
	//THE REST BOOT PARTITIONS
 	unsigned index = 1;
	while (index++ < 8) {
		if (device_partition_exist(supported_boot_partitions[index].name)){
			sprintf( menuTitle, "   KERNEL @ %s",
					supported_boot_partitions[index].name);
			sprintf( menuCommand, "boot_%i", index);
			add_menu_item(&main_menu, menuTitle	, menuCommand	, i++);
			j++;
		}
	}
	 */
	add_menu_item(&main_menu, "   RECOVERY"		, "boot_recv"	, i++);
	add_menu_item(&main_menu, "   FLASHLIGHT"	, "init_flsh"	, i++);
	add_menu_item(&main_menu, "   SETTINGS"		, "goto_sett"	, i++);
	add_menu_item(&main_menu, "   INFO"			, "goto_about"	, i++);
	add_menu_item(&main_menu, "   REBOOT"		, "acpu_ggwp"	, i++);
	add_menu_item(&main_menu, "   REBOOT cLK"	, "acpu_bgwp"	, i++);
	add_menu_item(&main_menu, "   POWERDOWN"	, "acpu_pawn"	, i++);
	main_menu.selectedi     = j; // FLASHLIGHT by default
	
	/*****************SETTINGS MENU*****************/
	int k=0;
    sett_menu.top_offset    = 0;
    sett_menu.bottom_offset = 0;
    sett_menu.maxarl		= 0;
    sett_menu.goback		= 0;
	sett_menu.id 			= SETT_MENU_ID;
    strcpy( sett_menu.MenuId, "SETTINGS" );
    strcpy( sett_menu.backCommand, "goto_main" );
    add_menu_item(&sett_menu, "   RESIZE PARTITIONS"	, "goto_rept",k++);
    add_menu_item(&sett_menu, "   REARRANGE PARTITIONS"	, "goto_rear",k++);
	// set add or remove extra boot partition according
	// to number of extra boot partitions (except boot)
	int idx = device_boot_ptn_num();
	if (idx == 0){idx++;} //fix when no extra boot partitions are present
	bool boot_idx_exist = device_partition_exist(supported_boot_partitions[idx].name);
	bool boot_next_idx_exist = device_partition_exist(supported_boot_partitions[idx+1].name);
	if (!boot_next_idx_exist){
		if (boot_idx_exist){
			sprintf( menuTitle, "   REMOVE %s",
					supported_boot_partitions[idx].name);
			sprintf( menuCommand, "xboot_dis_%i", idx);
		}else{
			sprintf( menuTitle, "   ADD %s",
					supported_boot_partitions[idx].name);
			sprintf( menuCommand, "xboot_en_%i", idx);
		}
		add_menu_item(&sett_menu, menuTitle, menuCommand, k++);
	}
	if (boot_idx_exist){
		if (boot_next_idx_exist){
			sprintf( menuTitle, "   REMOVE %s",
					supported_boot_partitions[idx+1].name);
			sprintf( menuCommand, "xboot_dis_%i", idx+1);
		}else{
			sprintf( menuTitle, "   ADD %s",
					supported_boot_partitions[idx+1].name);
			sprintf( menuCommand, "xboot_en_%i", idx+1);
		}
		if( strlen( supported_boot_partitions[idx+1].name ) != 0 )
			add_menu_item(&sett_menu, menuTitle, menuCommand, k++);
	}
	if (device_info.show_startup_info){
      	add_menu_item(&sett_menu, "   HIDE STARTUP INFO"	, "info_0", k++);
    }else{
      	add_menu_item(&sett_menu, "   SHOW STARTUP INFO"	, "info_1", k++);
    }
	if (device_info.multi_boot_screen == 1){
		add_menu_item(&sett_menu, "   HIDE MULTIBOOT MENU"	, "multiboot_0", k++);
	}else{
		add_menu_item(&sett_menu, "   SHOW MULTIBOOT MENU"	, "multiboot_1", k++);
	}
	add_menu_item(&sett_menu, "   SET DEFAULT KERNEL"   , "kernel_set",k++);
    if (msm_microp_i2c_status){
		add_menu_item(&sett_menu, "   SET DEFAULT SCREEN BRIGHTNESS"   	, "screenb_set", k++);
    }
	add_menu_item(&sett_menu, "   SET DEFAULT UDC SETTINGS"		, "udc_set",k++);
	add_menu_item(&sett_menu, "   SET DEFAULT OFF-MODE CHARGING" , "charge_set",k++);
	add_menu_item(&sett_menu, "   SET DEFAULT CHARGING VOLTAGE THRESHOLD" , "threshold_set",k++);
#if 0
    add_menu_item(&sett_menu, "   UPDATE RECOVERY"       	, "flash_recovery",k++);
#endif
	if (acpuclk_init_done != 0){
		add_menu_item(&sett_menu, "   SET DEFAULT CPU FREQ"	, "freq_set", k++);
	}
    if (device_info.extrom_enabled){
        add_menu_item(&sett_menu, "   DISABLE ExtROM"		, "extrom_disable",k++);
    }else{
        add_menu_item(&sett_menu, "   ENABLE ExtROM"		, "extrom_enable",k++);
    }
	if (device_info.fill_bbt_at_start){
		add_menu_item(&sett_menu, "   SKIP FILLING BBT @ STARTUP"	, "bbt_0", k++);
	}else{
		add_menu_item(&sett_menu, "   FILL BBT @ STARTUP"	, "bbt_1", k++);
	}	
	if (device_info.usb_detect){
      	add_menu_item(&sett_menu, "   DISABLE USB DETECTION"	, "usbd_0", k++);
    }else{
      	add_menu_item(&sett_menu, "   ENABLE USB DETECTION"	, "usbd_1", k++);
    }
	add_menu_item(&sett_menu, "   INVERT SCREEN COLORS"		, "invert_colors", k++);
	add_menu_item(&sett_menu, "   FORMAT NAND"		, "format_nand",k++);
	add_menu_item(&sett_menu, "   RESET SETTINGS"		, "reset_devinfo",k++);

	/*****************REPARTITION MENU*****************/
	rept_menu.id 		= REPT_MENU_ID;
	rept_sub_menu.id 	= REPT_MENU_ID;
	strcpy( rept_menu.MenuId, "RESIZE PARTITIONS" );
    strcpy( rept_menu.backCommand, "goto_sett" );
	
	/*****************REARRANGE MENU*****************/
	rear_menu.id 		= REAR_MENU_ID;
	rear_sub_menu.id 	= REAR_MENU_ID;
    strcpy( rear_menu.MenuId, "REARRANGE PARTITIONS" );
    strcpy( rear_menu.backCommand, "goto_sett" );
	
	/*****************INFO MENU*****************/
	int l=0;
	about_menu.top_offset    = 0;
    about_menu.bottom_offset = 0;
    about_menu.selectedi     = 0;
    about_menu.maxarl		 = 0;
    about_menu.goback		 = 0;
	about_menu.id			 = ABOUT_MENU_ID;
	strcpy( about_menu.MenuId, "INFO" );
	strcpy( about_menu.backCommand, "goto_main" );
    add_menu_item(&about_menu, "   PRINT PARTITION TABLE"	, "prnt_stat",l++);
	add_menu_item(&about_menu, "   PRINT BAD BLOCK TABLE"	, "prnt_bblocks",l++);
	add_menu_item(&about_menu, "   PRINT NAND INFO"		, "prnt_nand", l++);
    add_menu_item(&about_menu, "   CREDITS"			, "goto_credits", l++);
    add_menu_item(&about_menu, "   HELP"			, "goto_help", l++);
	 
	/*****************SHARED MENU*****************/
	shared_menu.id = SHARED_MENU_ID;
    strcpy( shared_menu.MenuId, "SHARED MENU" );
    strcpy( shared_menu.backCommand, "goto_main" );
		
    active_menu = &main_menu;
	redraw_menu();
}

static void ptentry_to_tag(unsigned **ptr, struct ptentry *ptn)
{
	struct atag_ptbl_entry atag_ptn;

	if (ptn->type == TYPE_MODEM_PARTITION)
		return;
		
	memcpy(atag_ptn.name, ptn->name, 16);
	atag_ptn.name[15] = '\0';
	atag_ptn.offset = ptn->start;
	atag_ptn.size = ptn->length;
	atag_ptn.flags = ptn->flags;
	memcpy(*ptr, &atag_ptn, sizeof(struct atag_ptbl_entry));
	*ptr += sizeof(struct atag_ptbl_entry) / sizeof(unsigned);
}

void boot_linux(void *kernel, unsigned *tags, 
				const char *cmdline, unsigned machtype,
				void *ramdisk, unsigned ramdisk_size)
{
	unsigned *ptr = tags;
	unsigned pcount = 0;
	void (*entry)(unsigned,unsigned,unsigned*) = kernel;
	struct ptable *ptable;
	int cmdline_len = 0;
	int have_cmdline = 0;
	int pause_at_bootup = 0;

	/* CORE */
	*ptr++ = 2;
	*ptr++ = 0x54410001;

	if (ramdisk_size) {
		*ptr++ = 4;
		*ptr++ = 0x54420005;
		*ptr++ = (unsigned)ramdisk;
		*ptr++ = ramdisk_size;
	}
	
	ptr = (unsigned *)target_atag(ptr);
	if( (ptable = flash_get_ptable()) && (ptable->count != 0) ) {
      	int i;
      	for(i=0; i < ptable->count; i++) {
      		struct ptentry *ptn;
      		ptn =  ptable_get(ptable, i);
      		if (ptn->type == TYPE_APPS_PARTITION)
      			pcount++;
      	}
      	*ptr++ = 2 + (pcount * (sizeof(struct atag_ptbl_entry) / sizeof(unsigned)));
      	*ptr++ = 0x4d534d70;
      	for (i = 0; i < ptable->count; ++i)
      		ptentry_to_tag(&ptr, ptable_get(ptable, i));
	}

	if (cmdline && cmdline[0]) {
		cmdline_len = strlen(cmdline);
		have_cmdline = 1;
	}
	if (target_pause_for_battery_charge()) {
		pause_at_bootup = 1;
		cmdline_len += strlen(battchg_pause);
	}

	if (cmdline_len > 0) {
		const char *src;
		char *dst;
		unsigned n;
		/* include terminating 0 and round up to a word multiple */
		n = (cmdline_len + 4) & (~3);
		*ptr++ = (n / 4) + 2;
		*ptr++ = 0x54410009;
		dst = (char *)ptr;
		if (have_cmdline) {
			src = cmdline;
			while ((*dst++ = *src++));
		}
		if (pause_at_bootup) {
			src = battchg_pause;
			if (have_cmdline) --dst;
			while ((*dst++ = *src++));
		}
		ptr += (n / 4);
	}

	/* END */
	*ptr++ = 0;
	*ptr++ = 0;

	printf( "Booting Linux ...\nkernel @ %p, ramdisk @ %p (%d)", kernel, ramdisk, ramdisk_size);
	if (cmdline)
		printf( ",\ncmdline:%s", cmdline);
		
	// If led is on turn it off
	if (htcleo_notif_led_mode != 0)
		thread_resume(thread_create("htcleo_notif_led_set_off",
									&htcleo_notif_led_set_mode,
									(void *)0,
									HIGH_PRIORITY,
									DEFAULT_STACK_SIZE));
#if HTCLEO_USE_CRAP_FADE_EFFECT
	mdelay(250);
#endif
	enter_critical_section();
	platform_exit();
	target_exit();
	entry(0, machtype, tags);
}

int boot_linux_from_flash(void)
{	
	struct boot_img_hdr *hdr = (void*) buf;
	unsigned n;
	struct ptentry *ptn;
	struct ptable *ptable;
	unsigned offset = 0;
	char *cmdline;
	char *err = "\n\nERROR: ";
	run_usbcheck = 0;
	
	ptable = flash_get_ptable();
	if (ptable == NULL) {
		strcat( err, "Partition table not found\n" );
		goto failed;
	}

	if (boot_into_recovery || target_pause_for_battery_charge()) { 
		// Boot from recovery
		printf("\n\nBooting to recovery ...\n\n");
		
        ptn = ptable_find(ptable, "recovery");
        if (ptn == NULL) {
	        strcat( err, "No recovery partition found\n" );
			boot_into_recovery=0;
	        goto failed;
        }
	} else {
		printf("\n\nBooting from %s partition ...\n\n",
				supported_boot_partitions[selected_boot].name);
		
        ptn = ptable_find(ptable, supported_boot_partitions[selected_boot].name);
        if (ptn == NULL) {
	        strcat( err, "No selected boot partition found\n" );
			goto failed;
        }
	}
	
	if (flash_read(ptn, offset, buf, page_size)) { 
		strcat( err, "Cannot read boot image header\n" );
		goto failed;
	}
	offset += page_size;

	if (memcmp(hdr->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE)) {
		strcat( err, "Invalid boot image header\n" );
		goto failed;
	}
	
#ifdef FORCE_BOOT_ADDR //currently undefined
	hdr->kernel_addr = KERNEL_ADDR;
	hdr->ramdisk_addr = RAMDISK_ADDR;
#endif

	if (hdr->page_size != page_size) {
		strcat( err, "Invalid boot image pagesize\n" );
		goto failed;
	}

	n = ROUND_TO_PAGE(hdr->kernel_size, page_mask);
	if (flash_read(ptn, offset, (void *)hdr->kernel_addr, n)) {
		strcat( err, "Cannot read kernel image\n" );
		goto failed;
	}
	offset += n;

	n = ROUND_TO_PAGE(hdr->ramdisk_size, page_mask);
	if (flash_read(ptn, offset, (void *)hdr->ramdisk_addr, n)) {
		strcat( err, "Cannot read ramdisk image\n" );
		goto failed;
	}
	offset += n;

	if (hdr->cmdline[0])
		cmdline = (char*) hdr->cmdline;
	else
		cmdline = (char*) target_get_cmdline();

	strcat(cmdline," clk=");
	strcat(cmdline,PSUEDO_VERSION);

#if TIMER_HANDLES_AUTOBOOT_EVENT
	timer_cancel(&autoboot);
#endif
#if TIMER_HANDLES_KEY_PRESS_EVENT
	timer_cancel(&key_listener_timer);
#else
	key_listen = 0;
#endif
		
	/* TODO: create/pass atags to kernel */
	boot_linux( (void *)hdr->kernel_addr, (void *)TAGS_ADDR,
				(const char *) cmdline, (unsigned)target_machtype(),
				(void *)hdr->ramdisk_addr, hdr->ramdisk_size );
	return 0;

failed:
	{
		if(fbcon_display() == NULL){
			htcleo_display_init();//in case of (re)booting not from cLK's menu
			fbcon_setfg(inverted ? 0x0000 : 0xffff);
		}
		printf("%s", err);
		printf("An irrecoverable error occured!!!\nDevice will reboot to bootloader in 5s.");
		mdelay(5000);
		target_reboot(FASTBOOT_MODE);
		return -1;
	}
}

void cmd_boot(const char *arg, void *data, unsigned sz)
{
	unsigned kernel_actual;
	unsigned ramdisk_actual __UNUSED;
	static struct boot_img_hdr hdr;
	char *ptr = ((char*) data);

	if (sz < sizeof(hdr)) {
		fastboot_fail("invalid bootimage header");
		return;
	}

	memcpy(&hdr, data, sizeof(hdr));

	/* ensure commandline is terminated */
	hdr.cmdline[BOOT_ARGS_SIZE-1] = 0;

	kernel_actual = ROUND_TO_PAGE(hdr.kernel_size, page_mask);
	ramdisk_actual = ROUND_TO_PAGE(hdr.ramdisk_size, page_mask);

	memmove((void*) KERNEL_ADDR, ptr + page_size, hdr.kernel_size);
	memmove((void*) RAMDISK_ADDR, ptr + page_size + kernel_actual, hdr.ramdisk_size);

	fastboot_okay("");
	target_battery_charging_enable(0, 1);
	udc_stop();

	boot_linux( (void*) KERNEL_ADDR, (void*) TAGS_ADDR,
				(const char*) hdr.cmdline, (unsigned)target_machtype(),
				(void*) RAMDISK_ADDR, hdr.ramdisk_size );
}

void cmd_erase(const char *arg, void *data, unsigned sz)
{
	struct ptable *ptable = flash_get_ptable();
	if (ptable == NULL)	{
		fastboot_fail("partition table doesn't exist");
		return;
	}
	
	struct ptentry *ptn = ptable_find(ptable, arg);
	if (ptn == NULL) {
		fastboot_fail("unknown partition name");
		return;
	}
	
	if (flash_erase(ptn)) {
		fastboot_fail("failed to erase partition");
		return;
	}

	selector_enable();
	fastboot_okay("");
}

void cmd_flash(const char *arg, void *data, unsigned sz)
{
	redraw_menu();

	struct ptable *ptable = flash_get_ptable();
	if (ptable == NULL) {
		fastboot_fail("partition table doesn't exist");
		return;
	}
	
	struct ptentry *ptn = ptable_find(ptable, arg);
	if (ptn == NULL) {
		fastboot_fail("unknown partition name");
		return;
	}
	
	if(!strcmp(ptn->name, "recovery")
	|| !strcmp(ptn->name, "boot")
	|| !strcmp(ptn->name, "sboot")
	|| !strcmp(ptn->name, "tboot")
	|| !strcmp(ptn->name, "vboot")
	|| !strcmp(ptn->name, "wboot")
	|| !strcmp(ptn->name, "xboot")
	|| !strcmp(ptn->name, "yboot")
	|| !strcmp(ptn->name, "zboot")) {
		if (memcmp((void *)data, BOOT_MAGIC, BOOT_MAGIC_SIZE)) {
			fastboot_fail("image is not a boot image");
			return;
		}
	}
	
	unsigned extra = 0;
	if (!strcmp(ptn->name, "system") || !strcmp(ptn->name, "userdata")) {
		extra = ((page_size >> 9) * 16);
	} else {
		sz = ROUND_TO_PAGE(sz, page_mask);
	}

	printf( "   writing %d bytes to '%s'...\n", sz, ptn->name);
	if (flash_write(ptn, extra, data, sz)) {
		fastboot_fail("flash write failure");
		return;
	}
	printf( "\n   partition '%s' updated", ptn->name);

	selector_enable();
	fastboot_okay("");
}

void cmd_continue(const char *arg, void *data, unsigned sz)
{
	active_menu=NULL;
	fbcon_resetdisp();
	if(inverted){draw_clk_header();fbcon_settg(0xffff);fbcon_setfg(0x0000);printf(" \n\n");}
	
	fastboot_okay("");
	target_battery_charging_enable(0, 1);
	udc_stop();
	selected_boot = 0;
	boot_into_recovery = 0;
	boot_linux_from_flash();
}

void cmd_reboot(const char *arg, void *data, unsigned sz)
{
	redraw_menu();

	fastboot_okay("");
	target_reboot(0);
}

void cmd_reboot_bootloader(const char *arg, void *data, unsigned sz)
{
	redraw_menu();

	fastboot_okay("");
	target_reboot(FASTBOOT_MODE);
}

void cmd_oem_rec_boot(void)
{
	active_menu=NULL;
	fbcon_resetdisp();
    if(inverted){draw_clk_header();fbcon_settg(0xffff);fbcon_setfg(0x0000);printf(" \n\n");}
	
	fastboot_okay("");
	target_battery_charging_enable(0, 1);
	udc_stop();
    boot_into_recovery = 1;
    boot_linux_from_flash();
}

void cmd_powerdown(const char *arg, void *data, unsigned sz)
{
	redraw_menu();

	fastboot_okay("");
	target_shutdown();
	thread_exit(0);
}

void send_mem(char* start, int len)
{
	char response[64];
	while(len>0)
	{
		int slen = len > 29 ? 29 : len;
		memcpy(response, "INFO", 4);
		response[4]=slen;

		for(int i=0; i<slen; i++)
		{
			response[5+i*2] = charVal(start[i]>>4);
			response[5+i*2+1]= charVal(start[i]);
		}
		response[5+slen*2+1]=0;
		fastboot_write(response, 5+slen*2);

		start+=slen;
		len-=slen;
	}
}

void cmd_oem_smesg()
{
	redraw_menu();

	send_mem((char*)0x1fe00018, MIN(0x200000, readl(0x1fe00000)));

	selector_enable();
	fastboot_okay("");
}

void cmd_oem_dmesg()
{
	redraw_menu();

	if(*((unsigned*)0x2FFC0000) == 0x43474244 /* DBGC */  ) //see ram_console_buffer in kernel ram_console.c
	{
		send_mem((char*)0x2FFC000C, *((unsigned*)0x2FFC0008));
	}

	selector_enable();
	fastboot_okay("");
}

void cmd_oem_dumpmem(const char *arg)
{
	redraw_menu();

	char *sStart = strtok((char*)arg, " ");
	char *sLen = strtok(NULL, " ");
	if(sStart==NULL || sLen==NULL)
	{
		fastboot_fail("usage:oem dump start len");
		return;
	}

	send_mem((char*)str2u(sStart), str2u(sLen));

	selector_enable();
	fastboot_okay("");
}

void cmd_oem_set(const char *arg)
{
	redraw_menu();

	char type=*arg; arg++;
	char *sAddr = strtok((char*) arg, " ");
	char *sVal = strtok(NULL, "\0");
	if(sAddr==NULL || sVal==NULL)
	{
		fastboot_fail("usage:oem set[s,c,w] address value");
		return;
	}
	char buff[64];
	switch(type)
	{
		case 's':
			memcpy((void*)str2u(sAddr), sVal, strlen(sVal));
			send_mem((char*)str2u(sAddr), strlen(sVal));
			break;
		case 'c':
			*((char*)str2u(sAddr)) = (char)str2u(sVal);
			sprintf(buff, "%x", *((char*)str2u(sAddr)));
			send_mem(buff, strlen(buff));
			break;
		case 'w':
		default:
			*((int*)str2u(sAddr)) = str2u(sVal);
			sprintf(buff, "%x", *((int*)str2u(sAddr)));
			send_mem(buff, strlen(buff));
	}

	selector_enable();
	fastboot_okay("");
}

void cmd_oem_cls()
{
	redraw_menu();

	selector_enable();
	fastboot_okay("");
}

void cmd_oem_part_add(const char *arg)
{
	redraw_menu();
	
	device_add(arg);
	device_list();

	selector_enable();
	fastboot_okay("");
}

void cmd_oem_part_resize(const char *arg)
{
	redraw_menu();
	
	device_resize(arg);
	device_list();

	selector_enable();
	fastboot_okay("");
}

void cmd_oem_part_del(const char *arg)
{
	redraw_menu();
	
	device_del(arg);
	device_list();

	selector_enable();
	fastboot_okay("");
}

void cmd_oem_part_commit()
{
	device_commit();

	redraw_menu();

	printf("\n   Partition changes saved! Device will reboot in 2s.\n");
	fastboot_okay("");
	mdelay(2000);
	target_reboot(FASTBOOT_MODE);
}

void cmd_oem_part_read()
{
	redraw_menu();
	
	device_read();
	device_list();

	selector_enable();
	fastboot_okay("");
}

void cmd_oem_part_list()
{
	redraw_menu();

	device_list();

	selector_enable();
	fastboot_okay("");
}

void cmd_oem_part_clear()
{
	redraw_menu();
	
	device_clear();
	device_list();

	selector_enable();
	fastboot_okay("");
}

void cmd_oem_part_create_default()
{
	redraw_menu();
	
	device_create_default();
	device_list();

	selector_enable();
	fastboot_okay("");
}

void prnt_nand_stat(void)
{
	if ( flash_info == NULL ) {
		printf( "   ERROR: flash info unavailable!!!\n" );
		return;
	}
	
	struct ptable *ptable = flash_get_devinfo();
	if ( ptable == NULL ) {
		printf( "   ERROR: DEVINFO not found!!!\n" );
		return;
	}
	
	struct ptentry* ptn = ptable_find( ptable, PTN_DEV_INF );
	if ( ptn == NULL ) {
		printf( "   ERROR: DEVINFO not found!!!\n" );
		return;
	}

	printf("    ____________________________________________________ \n\
		   |                      NAND INFO                     |\n\
		   |____________________________________________________|\n" );
	printf("   | ID: 0x%x MAKER: 0x%2x   DEV: 0x%2x TYPE: %dbit |\n",
		flash_info->id, flash_info->vendor, flash_info->device, (flash_info->type)*8);
	printf("   | BT MAC: %s    CID: %8s         |\n", device_btmac, device_cid);
	printf("   | WIFI MAC: %s  IMEI: %s |\n", device_wifimac, device_imei);
	printf("   |====================================================|\n");
	printf("   | Flash   block   size: %22i bytes |\n", flash_info->block_size);
	printf("   | Flash   page    size: %22i bytes |\n", flash_info->page_size);
	printf("   | Flash   spare   size: %22i bytes |\n", flash_info->spare_size);
	printf("   | Flash   total   size: %4i block(s)  = %5i MB    |\n",
		flash_info->num_blocks, (int)(flash_info->num_blocks/get_blk_per_mb()));
	printf("   |====================================================|\n");
	printf("   | ROM     (0x%x) size: %4i block(s)  = %5i MB    |\n",
		HTCLEO_ROM_OFFSET, get_usable_flash_size(), (int)(get_usable_flash_size()/get_blk_per_mb()));
	printf("   | DEVINFO (0x%x) size: %4i block(s)  = %5i KB    |\n",
		ptn->start, ptn->length, (flash_info->block_size/1024));
	printf("   | PTABLE  (0x%x) size: %4i block(s)  = %5i MB    |\n",
		get_flash_offset(), get_full_flash_size(), (int)(get_full_flash_size()/get_blk_per_mb()));
	printf("   | ExtROM  (0x%x) size: %4i block(s)  = %5i MB    |\n",
		get_ext_rom_offset(), get_ext_rom_size(), round((float)(get_ext_rom_size())/(float)(get_blk_per_mb())));
	printf("   |____________________________________________________|\n");
#if 0 //set to 1 when ds2746 works
	printf("   |                                                    |\n");
	printf("   |      Voltage : %4i mV  --  Current : %4i mA      |\n", ds2746_voltage(DS2746_I2C_SLAVE_ADDR), ds2746_current(DS2746_I2C_SLAVE_ADDR, 1200));
	printf("   |____________________________________________________|\n");
#endif
	//ptable_dump(flash_get_ptable());
}

void cmd_oem_nand_status(void)
{
	redraw_menu();

	prnt_nand_stat();

	selector_enable();
	fastboot_okay("");
}

void cmd_oem_part_format_all()
{
	redraw_menu();
	
	printf("   Initializing flash format...\n");
	struct ptable *ptable = flash_get_devinfo();
	if (ptable == NULL) {
		printf( "   ERROR: DEVINFO not found!!!\n");
		return;
	}
	
	struct ptentry *ptn = ptable_find(ptable, "task29");
	if (ptn == NULL) {
		printf( "   ERROR: DEVINFO not found!!!\n");
		return;
	}

	printf("   Formating flash...\n");	
	if (flash_erase(ptn)) {
		fastboot_fail("failed to erase partition");
		return;
	}	
	printf("\n   Format complete !\n   Reboot device to create default partition table,\n   or create partitions manualy!\n");
	
	selector_enable();
	fastboot_okay("");
}

void cmd_oem_part_format_devinfo()
{	
	redraw_menu();
	
	struct ptable *ptable = flash_get_devinfo();
	if (ptable == NULL)	{
		printf( "   ERROR: DEVINFO not found!!!\n");
		return;
	}
	
	struct ptentry *ptn = ptable_find(ptable, "devinf");
	if (ptn == NULL) {
		printf( "   ERROR: No DEVINFO section!!!\n");
		return;
	}
	
	printf("   Wiping devinfo...\n");
	if (flash_erase(ptn)) {
		fastboot_fail("failed to erase partition");
		return;
	}
		
	printf("   Settings were reset !\n   Device will reboot in 3s to load default settings...");
	fastboot_okay("");
	mdelay(3000);
	target_reboot(FASTBOOT_MODE);
}

void cmd_flashlight(void)
{	
#if TIMER_HANDLES_KEY_PRESS_EVENT
	// We need a way to turn off flashlight
	// Lets try using a key_listener thread just for the time flashlight is on
	thread_resume(thread_create("start_keylistener",
								&key_listener,
								NULL,
								DEFAULT_PRIORITY,
								DEFAULT_STACK_SIZE));
#endif
	thread_resume(thread_create("flashlight",
								(thread_start_routine)&target_flashlight,
								NULL,
								DEFAULT_PRIORITY,
								DEFAULT_STACK_SIZE));
	return;
}

void oem_help()
{
	fbcon_settg((inverted ? 0xffff : 0x0000));
	fbcon_setfg((inverted ? 0x0000 : 0xffff));
	printf("\n");
	printf("=> fastboot oem key x (where x=8,2,5,0)\n   Simulate keys using fastboot(8:UP,2:DOWN,5:YES,0:BACK)\n");
	printf("=> fastboot oem set[c,w,s] addr value\n   Set char(1byte), word(4 bytes), or string\n");
	printf("=> fastboot oem pwf addr len\n   Dump memory\n");
	printf("=> fastboot oem boot-recovery\n   Boot from recovery\n");
	printf("=> fastboot oem dmesg\n   Kernel debug messages\n");
	printf("=> fastboot oem smesg\n   Spl messages\n");
	printf("=> fastboot oem poweroff\n   Powerdown\n");
	printf("=> fastboot oem nandstat\n   Print nand info\n");
	printf("=> fastboot oem part-list\n   Display current partition layout\n");
	printf("=> fastboot oem part-add name:size\n   Create new partition with given name and size in MB\n");
	printf("=> fastboot oem part-add name:size:b\n   Create new partition with given name and size in blocks\n");
	printf("=> fastboot oem part-del name\n   Delete named partition\n");
	printf("=> fastboot oem part-read\n   Reload partition layout from NAND\n");
	printf("=> fastboot oem part-clear\n   Clear current partition layout\n");
	printf("=> fastboot oem part-commit\n   All partition changes are TEMP until this cmd is issued\n");
	printf("=> fastboot oem part-resize name:size\n   Resize partition with given name to new size in MB\n");
	printf("=> fastboot oem part-resize name:size:b\n   Resize partition with given name to new size in blocks\n");
	printf("=> fastboot oem part-create-default\n   Delete current partition table and create default one\n");
	printf("=> fastboot oem format-all\n   Wipe complete nand (except clk)\n");
	printf("=> fastboot oem reset\n   Reset settings (wipe DEVINFO)\n");
	printf("=> fastboot flash lk lk.img\n   Update clk through fastboot using proper image file");
}
void cmd_oem_help()
{
	shared_menu.top_offset		= 0;
	shared_menu.bottom_offset	= 0;
	shared_menu.selectedi		= 0;
	shared_menu.maxarl			= 0;
	shared_menu.goback			= 0;
	strcpy( shared_menu.MenuId, "HELP" );
	strcpy( shared_menu.backCommand, "goto_about" );
	add_menu_item(&shared_menu, ">> HELP (fastboot oem help)", "goto_about", 0);
		
	fbcon_clear_region(0, 45 , (inverted ? 0xffff : 0x0000));
	fbcon_set_y_cord(0);fbcon_set_x_cord(0);
		
	active_menu = &shared_menu;
	_dputs(active_menu->item[0].mTitle);
	_dputs("\n");
	selector_enable();
	fastboot_okay("");
	oem_help();
}

/*
 * koko: Credits added in Main Menu/Info items
 */
void cmd_oem_credits(void)
{
	fbcon_setfg(inverted ? 0x0000 : 0xffff);
	printf("   xda-developers.com for being an educational portal,\n");
	printf("   bepe, Cotulla and DFT for the HardSPL and their over-all\n\
		   contribution to HD2(Android, WP7, etc.),\n");
	printf("   LeTama for his work on the NAND driver,\n");
	printf("   Martijn Stolk for his kernel segfault solving code,\n");
	printf("   Travis Geiselbrecht for writing (L)ittle (K)ernel,\n");
	printf("   CodeAurora & QC for adding HW level support for qsd8250,\n");
	printf("   cedesmith for his great work on porting LK to HD2\n\
		   and the ppp wrappers,\n");
	printf("   Martin Johnson for writing tinboot,\n");
	printf("   arif-ali for the version patch and his HowTo,\n");
	printf("   Dan1j3l for the offmode charging daemon in recovery\n\
		   and for the dynamic partition table,\n");
	printf("   xdmcdmc for his work on nbgen(recovery added in NBH)\n\
		   and on partition table(repartition on the fly),\n");
	printf("   Alexander Tarasikov for his work on 'pooploader',\n");
	printf("   Rick_1995 for his universal work on maintaining ori-\n\
		   ginal cLK, further developing LK by adding great new\n\
		   features and sharing his knowledge with all XDAers,\n");
	printf("   seadersn for helping with the original compilation,\n");
	printf("   spalm for the wp7 style android HD2 bootanimation\n\
		   from which came the htc HD2 logo,\n");
	printf("   stirkac for his fantastic documentation on HowTo,\n");
	printf("   ALL who worked on making linux kernel possible on hd2.\n");
}

/*
 * koko : Navigate through lk's menu using fastboot cmds
 *		  in case of broken hardware keys.
 *		  'fastboot oem key 8' ==> KEY_VOLUMEUP
 *		  'fastboot oem key 2' ==> KEY_VOLUMEDOWN
 *		  'fastboot oem key 5' ==> KEY_SEND
 *		  'fastboot oem key 0' ==> KEY_BACK
 */
void cmd_oem_key(const char *arg)
{
	switch(*arg)
	{
		case '8':
			menu_item_nav(KEY_VOLUMEUP);
			break;
		case '2':
			menu_item_nav(KEY_VOLUMEDOWN);
			break;
		case '5':
			eval_command();
			break;
		case '0':
			active_menu->goback = 1;
			eval_command();
			break;
		default:
			break;
	}

	fastboot_okay("");
}

/*
 * koko : Attempt to mark specific block as bad
 *		  'fastboot oem mark-block boot:911'
 */
void cmd_oem_mark_block(const char *arg)
{
	redraw_menu();
	
	char buff[64];
	char ptn_name[32];
	char* tmp_buff;
	unsigned block;

	strcpy( buff, arg );

	tmp_buff = strtok( buff, ":" );
	strcpy( ptn_name, tmp_buff );
	tmp_buff = strtok( NULL, ":" );
	block = atoi( tmp_buff );

	struct ptable *ptable = flash_get_ptable();
	if (ptable == NULL)	{
		fastboot_fail("partition table doesn't exist");
		return;
	}
	
	struct ptentry *ptn = ptable_find(ptable, ptn_name);
	if (ptn == NULL) {
		fastboot_fail("unknown partition name");
		return;
	}
	
	flash_mark_badblock(ptn, block);
	
	selector_enable();
	fastboot_okay("");
}

/*
 * koko : Removed flashlight and test from cmds - added boot recovery
 */
void cmd_oem(const char *arg, void *data, unsigned sz)
{
	while(*arg==' ') arg++;
	if(memcmp(arg, "cls", 3)==0)                           cmd_oem_cls();
	if(memcmp(arg, "set", 3)==0)                           cmd_oem_set(arg+3);
	if(memcmp(arg, "pwf ", 4)==0)                          cmd_oem_dumpmem(arg+4);
	if(memcmp(arg, "key ", 4)==0)                          cmd_oem_key(arg+4);
	if(memcmp(arg, "mark-block ", 11)==0)                  cmd_oem_mark_block(arg+11);
	if(memcmp(arg, "boot-recovery", 13)==0)                cmd_oem_rec_boot();
	if(memcmp(arg, "dmesg", 5)==0)                         cmd_oem_dmesg();
	if(memcmp(arg, "smesg", 5)==0)                         cmd_oem_smesg();
	if(memcmp(arg, "nandstat", 8)==0)                      cmd_oem_nand_status();
	if(memcmp(arg, "poweroff", 8)==0)                      cmd_powerdown(arg+8, data, sz);
	if(memcmp(arg, "part-add ", 9)==0)                     cmd_oem_part_add(arg+9);
	if(memcmp(arg, "part-del ", 9)==0)                     cmd_oem_part_del(arg+9);
	if(memcmp(arg, "part-read", 9)==0)                     cmd_oem_part_read();
	if(memcmp(arg, "part-list", 9)==0)                     cmd_oem_part_list();
	if(memcmp(arg, "part-clear", 10)==0)                   cmd_oem_part_clear();
	if(memcmp(arg, "format-all", 10)==0)                   cmd_oem_part_format_all();
	if(memcmp(arg, "part-commit", 11)==0)                  cmd_oem_part_commit();
	if(memcmp(arg, "part-resize ", 12)==0)                 cmd_oem_part_resize(arg+12);
	if(memcmp(arg, "reset", 5)==0)               		   cmd_oem_part_format_devinfo();
	if(memcmp(arg, "part-create-default", 19)==0)          cmd_oem_part_create_default();
	if((memcmp(arg,"help",4)==0)||(memcmp(arg,"?",1)==0))  cmd_oem_help();
}

#define EXPAND(NAME) 	#NAME
#define TARGET(NAME) 	EXPAND(NAME)
void htcleo_fastboot_init(void)
{
	// Initiate Fastboot.
	fastboot_register("oem", cmd_oem);
	fastboot_register("boot", cmd_boot);
	fastboot_register("flash:", cmd_flash);
	fastboot_register("erase:", cmd_erase);
	fastboot_register("continue", cmd_continue);
	fastboot_register("reboot", cmd_reboot);
	fastboot_register("reboot-bootloader", cmd_reboot_bootloader);
	fastboot_register("powerdown", cmd_powerdown);
	fastboot_publish("version", "1.0");
	fastboot_publish("version-bootloader", "1.0");
	fastboot_publish("version-baseband", "unset");
	fastboot_publish("serialno", "HTC HD2");
	fastboot_publish("secure", "no");
	fastboot_publish("product", TARGET(BOARD));
	fastboot_publish("kernel", "lk");
	fastboot_publish("author", "Shantanu Gupta, Danijel Posilovic, Arif Ali, Cedesmith, Qualcomm Innovation Centre, Travis Geiselbrecht.");
	fastboot_init(target_get_scratch_address(), target_get_scratch_size());
	udc_start();
	target_battery_charging_enable(1, 0);
}

static int bbtbl(void *arg)
{
	thread_preempt();
	flash_bad_blocks = flash_bad_block_table(ptable_find(flash_get_devinfo(), "task29"));
	//dprintf(SPEW, "Finished filling bad block table\n");
	thread_exit(0);
	return 0;
}

static int update_ver_str(void *arg)
{
	update_spl_ver();
	update_radio_ver();
	update_device_imei();
	update_device_cid();
	update_device_wifimac();
	update_device_btmac();

	thread_exit(0);
	return 0;
}

uint32_t get_code_from_reboot_mode(uint32_t reboot_mode, uint32_t mark) {
	char str[32], *endptr;
	sprintf(str, "%x", (reboot_mode ^ mark));
	return strtol(str, &endptr, 10);
}

void aboot_init(const struct app_descriptor *app)
{
	flash_info = flash_get_info();
	page_size = flash_page_size();
	page_mask = page_size - 1;
	show_multi_boot_screen = -1;
	run_usbcheck = 0;
	key_listen = 1;
	//suspend_time = 1 => infinite suspend for off-mode charge purpose
	//suspend_time > 1 => suspend for specific time (while able to charge)
	suspend_time = 1;
	selected_boot = device_info.boot_sel;
	uint32_t MinutesToSuspend = 0;
	/*******************************************************
	 *Check if we should do something other than booting up.
	 *******************************************************/
// FIRST check if user pressed any key assigned to certain action
	// [KEY_HOME] pressed => boot recovery
	if (keys_get_state(keys[6]) != 0) {
		htcleo_vibrate_once();
		show_multi_boot_screen = 0;
		boot_into_recovery = 1;
	} else
	// [KEY_BACK] pressed => clk menu
	if (keys_get_state(keys[5]) != 0) {
		htcleo_vibrate_once();
		show_multi_boot_screen=0;
		goto bmenu;
	} else
	// [KEY_SOFT1] pressed => boot sboot
 	if (keys_get_state(keys[2]) != 0) {
		htcleo_vibrate_once();
		show_multi_boot_screen = 0;
		boot_into_recovery = 0;
		selected_boot = device_partition_exist("sboot");
	} /* else */
	
// SECONDLY check the boot reason
	// RECOVERY_MODE => boot recovery
 	if (target_check_reboot_mode() == RECOVERY_MODE) {
		show_multi_boot_screen = 0;
 		boot_into_recovery = 1;
 	} else
	// FASTBOOT_MODE => boot cLK menu
	if(target_check_reboot_mode() == FASTBOOT_MODE) {
		show_multi_boot_screen = 0;
		goto bmenu;
	} else
	// Exploiting MARK_OEM_TAG for directly booting any extra 'boot' partition
	if((target_check_reboot_mode() & 0xFFFFFF00) ==  MARK_OEM_TAG) {
		show_multi_boot_screen = 0;
		boot_into_recovery = 0;
		// 0 <= selected_boot <= 7
		selected_boot = get_code_from_reboot_mode(target_check_reboot_mode(), MARK_OEM_TAG);
	} else
	/*
	// Exploiting MARK_OEM_TAG => Suspend device for given time
	if((target_check_reboot_mode() & 0xFFFFFF00) ==  MARK_OEM_TAG) {
		// Get MinutesToSuspend from the reboot_mode
		MinutesToSuspend = get_code_from_reboot_mode(target_check_reboot_mode(), MARK_OEM_TAG);
		if (MinutesToSuspend > 3)
			MinutesToSuspend -= 2;
			
		suspend_time = (time_t)(MinutesToSuspend * 60000);
		show_multi_boot_screen = 0;
		boot_into_recovery = 0;
	} else
	*/
	// Exploiting MARK_ALARM_TAG => Suspend device for given time
	if((target_check_reboot_mode() & 0xFF000000) == MARK_ALARM_TAG) {
		// Get MinutesToSuspend from the reboot_mode
		MinutesToSuspend = get_code_from_reboot_mode(target_check_reboot_mode(), MARK_ALARM_TAG);
		if (MinutesToSuspend > 3)
			MinutesToSuspend -= 2;

		suspend_time = (time_t)(MinutesToSuspend * 60000);
		show_multi_boot_screen = 0;
		boot_into_recovery = 0;
	}
	
// Check if we must pause for inbuilt offmode charging OR suspend mode
	if (htcleo_pause_for_battery_charge  || suspend_time > 1) {
		msm_i2c_init();
		msm_microp_i2c_init();
		htcleo_usb_init();
		
		thread_resume(thread_create("htcleo_suspend",
									&htcleo_suspend,
									(void *)suspend_time,
									HIGHEST_PRIORITY,
									DEFAULT_STACK_SIZE));
		return;
	}
	
// Handle misc command page according to value of boot_into_recovery
	if (boot_into_recovery == 1)
		recovery_init();

// Check whether to show multiboot menu or not.
// show_multi_boot_screen will still be negative if none of the above checks changed it.
// This may happen if we just (re)booted with no interaction.
	if (show_multi_boot_screen < 0) {
		if (device_info.multi_boot_screen == 1){
			show_multi_boot_screen = (device_boot_ptn_num() > 0 ? 1 : 0);
			goto bmenu;
		}
	}
// DONE CHECKING!
	
	/********************************************************
	 * Environment setup for continuing, Lets boot if we can.
	 ********************************************************/
 	boot_linux_from_flash();

	/**********************************************************
	 * Couldn't Find anything to do (OR) User pressed Back Key.
	 * Load appropriate menu.
	 **********************************************************/
bmenu:
	{		
		// Turn keys backlight ON
		htcleo_key_bkl_pwr(1);
		
		if(show_multi_boot_screen==0) { //Show Main Menu
			/*
			 * koko: For now adding here i2c probing to avoid 
			 *		 delaying the load of the kernel.
			 *		 After all enabling i2c bus won't offer us
			 *		 anything if we just want to boot up android,
			 *		 since it will be enabled and then disabled 
			 *		 without any effect or specific purpose.
			 *			 
			I2C probe  			   	Microp_I2C probe	*/
			msm_i2c_init();			msm_microp_i2c_init();
					
			// SDCard init
			/*thread_resume(thread_create("htcleo_mmc_init",
										&htcleo_mmc_init,
										NULL,
										DEFAULT_PRIORITY,
										DEFAULT_STACK_SIZE));*/
										
			if(device_info.fill_bbt_at_start){
				thread_resume(thread_create("fill_bad_block_table",
											&bbtbl,
											NULL,
											HIGHEST_PRIORITY,
											DEFAULT_STACK_SIZE));
			}
			
			thread_resume(thread_create("update_nand_info",
										&update_ver_str,
										NULL,
										HIGH_PRIORITY,
										DEFAULT_STACK_SIZE));

			htcleo_usb_init();
			htcleo_udc_init();
			htcleo_fastboot_init();
			usb_charger_change_state();
			
			htcleo_display_init();
			init_menu();
			
			if(device_info.show_startup_info){
				device_list();
			}
			
			// Light sensor init
			//htcleo_ls_init();
			
			// Touch screen init
			//htcleo_ts_init();
			
#if TIMER_HANDLES_KEY_PRESS_EVENT
			enter_critical_section();
			timer_initialize(&key_listener_timer);
			timer_set_oneshot(&key_listener_timer, 0, key_listener_tick, NULL);
			exit_critical_section();
#else
			keyp_thread = thread_create("start_keylistener",
										&key_listener,
										NULL,
										DEFAULT_PRIORITY,
										DEFAULT_STACK_SIZE);
			thread_resume(keyp_thread);
#endif		
			if(device_info.usb_detect){
				run_usbcheck = 1;
				thread_resume(thread_create("usb_listener",
											&usb_listener,
											NULL,
											DEFAULT_PRIORITY,
											DEFAULT_STACK_SIZE));
			}
			
			selector_enable();
			
			// Turn keys backlight OFF
			htcleo_key_bkl_pwr(0);
		} else { // Show multiboot menu
			htcleo_display_init();
			init_multiboot_menu();
		}		
	}
}

APP_START(aboot)
	.init = aboot_init,
	.flags = APP_FLAG_CUSTOM_STACK_SIZE,
	.stack_size = 8192,
APP_END
