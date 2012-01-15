/*
 * menu.c
 * This file is part of Little Kernel
 *
 * Copyright (C) 2011 - Shantanu Gupta
 *
 * Little Kernel is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Little Kernel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Little Kernel; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */


 
#include <arch/arm.h>
#include <arch/ops.h>
#include <platform/iomap.h>
#include <platform/timer.h>
#include <sys/types.h>
#include <app.h>
#include <bits.h>
#include <compiler.h>
#include <debug.h>
#include <err.h>
#include <hsusb.h>
#include <platform.h>
#include <reg.h>
#include <smem.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <dev/flash.h>
#include <dev/fbcon.h>
#include <dev/keys.h>
#include <dev/udc.h>
#include <lib/ptable.h>
#include <lib/devinfo.h>
#include <kernel/thread.h>
#include <kernel/timer.h>

#include "aboot.h"
#include "bootimg.h"
#include "fastboot.h"
#include "recovery.h"
#include "oem.h"
#include "menu.h"

static uint16_t keyp;
static uint16_t keys[] = {KEY_VOLUMEUP, KEY_VOLUMEDOWN, KEY_SOFT1, KEY_SEND, KEY_CLEAR, KEY_BACK, KEY_HOME};

//unsigned draw_fastboot_flag = 0;

void selector_disable(void)
{
	int sel_current_y_offset = fbcon_get_y();
	int sel_current_x_offset = fbcon_get_x();
	
	fbcon_set_x(active_menu->item[active_menu->selectedi].x);
	fbcon_set_y(active_menu->item[active_menu->selectedi].y);
	
	fbcon_forcetg(true);
	fbcon_init_colors();
	
	_dputs(active_menu->item[active_menu->selectedi].mTitle);
	
	fbcon_forcetg(false);
	fbcon_set_y(sel_current_y_offset);
	fbcon_set_x(sel_current_x_offset);
}
void selector_enable(void)
{
	int sel_current_y_offset = fbcon_get_y();
	int sel_current_x_offset = fbcon_get_x();
	
	fbcon_set_x(active_menu->item[active_menu->selectedi].x);
	fbcon_set_y(active_menu->item[active_menu->selectedi].y);
	
	fbcon_set_colors(0, 1, 1, 0x0000, 0xffff, 0x001f);
	
	_dputs(active_menu->item[active_menu->selectedi].mTitle);
	
	fbcon_init_colors();
	fbcon_set_y(sel_current_y_offset);
	fbcon_set_x(sel_current_x_offset);
}
void draw_menu_header(void){
	fbcon_set_colors(0, 1, 0, 0x0000, 0x02e0, 0x0000);
	printf("\n   LEO100 HX-BC SHIP S-OFF\n   HBOOT-1.5.0.0\n   MICROP-0x30\n   SPL-CotullaHSPL 0x30\n   RADIO-\n   Jan 18 2012, 18:19:95\n\n\n");
	fbcon_set_colors(0, 1, 1, 0x0000, 0xc3a0, 0xffff);
	_dputs("   <VOL UP> to previous item\n   <VOL DOWN> to next item\n   <CALL> to select item\n   <BACK> to return\n\n\n");
	if (active_menu != NULL)
	{
		_dputs("   ");
		fbcon_set_colors(0, 1, 1, 0x0000, 0xffff, 0x001f);
		_dputs(active_menu->id);
		_dputs("\n\n\n");
		fbcon_set_colors(0, 1, 1, 0x0000, 0x0000, 0xffff);
	}
}
void draw_menu(void)
{
	fbcon_reset();
	draw_menu_header();
	for (uint8_t i = 0 ;; i++){
		if ((strlen(active_menu->item[i].mTitle) != 0) && !(i > active_menu->maxarl) )
		{
			_dputs("   ");
			if(active_menu->item[i].x == ERR_NOT_VALID)
				active_menu->item[i].x = fbcon_get_x();
			if(active_menu->item[i].y == ERR_NOT_VALID)
				active_menu->item[i].y = fbcon_get_y();
			_dputs(active_menu->item[i].mTitle);
			_dputs("\n");
		} else break;
	}
	_dputs("\n");
	selector_enable();
}
static void menu_item_down()
{
	if (didyouscroll()) draw_menu();
	
	selector_disable();
	
	if (active_menu->selectedi == (active_menu->maxarl-1))
		active_menu->selectedi=0;
	else
		active_menu->selectedi++;
	
	selector_enable();
}
static void menu_item_up()
{
	if (didyouscroll())	draw_menu();
	
	selector_disable();
	
	if ((active_menu->selectedi) == 0)
		active_menu->selectedi=(active_menu->maxarl-1);
	else
		active_menu->selectedi--;
	
	selector_enable();
}
void add_menu_item(struct menu *xmenu,const char *name,const char *command)
{
	if(xmenu->maxarl==MAX_MENU)
	{
		printf("Menu: is overloaded with entry %s",name);
		return;
	}
	
	strcpy(xmenu->item[xmenu->maxarl].mTitle,name);
	strcpy(xmenu->item[xmenu->maxarl].command,command);
	
	xmenu->item[xmenu->maxarl].x = ERR_NOT_VALID;
	xmenu->item[xmenu->maxarl].y = ERR_NOT_VALID;
	
	xmenu->maxarl++;
	return;
}
int flashlight(void *arg)
{
	// Code picked from htc-linux.org
	volatile unsigned *bank6_p1= (unsigned int*)(0xA9000864);
	volatile unsigned *bank6_p2= (unsigned int*)(0xA9000814);
	for(;;)
	{
		*bank6_p2=*bank6_p1 ^ 0x200000;
		if(keys_get_state(0x123)!=0)break;
		udelay(498);
	}
	thread_exit(0);
}
void cmd_flashlight(void)
{
	thread_resume((thread_t *)thread_create("Flashlight", &flashlight, NULL, HIGHEST_PRIORITY, DEFAULT_STACK_SIZE));
}

void eval_command( uint8_t stage)
{
	char command[CMD_SZ];
	if (stage == 2)
	{
		strcpy(command, active_menu->item[active_menu->selectedi].command);
	} else {
		strcpy(command, active_menu->back_cmd);
	}

	if (!memcmp(command,"boot_recv", strlen(command)))
	{
		boot_into_recovery = 1;
		if (boot_linux_from_flash() == -1){draw_menu();}
		return;
	} else if (!memcmp(command,"prnt_clrs", strlen(command))){
		draw_menu();
		return;	
	} else if (!memcmp(command,"boot_nand", strlen(command))){
		boot_into_recovery = 0;
		if (boot_linux_from_flash() == -1){draw_menu();}
		return;
	}else if (!memcmp(command,"goto_rept", strlen(command))){
		printf("\n   ERROR!: Feature not compiled");
		return;
	}else if (!memcmp(command,"goto_sett", strlen(command))){
		active_menu = &sett_menu;
		draw_menu();
		return;
	}else if (!memcmp(command,"goto_main", strlen(command))){
		active_menu = &main_menu;
		draw_menu();
		return;
	}else if (!memcmp(command,"acpu_ggwp", strlen(command))){
		reboot_device(0);
		return;
	}else if (!memcmp(command,"acpu_bgwp", strlen(command))){
		reboot_device(FASTBOOT_MODE);
		return;
	}else if (!memcmp(command,"acpu_pawn", strlen(command))){
		shutdown();
		return;
	}else if (!memcmp(command,"enable_extrom", strlen(command))){
		device_enable_extrom();
		printf("Will Auto-Reboot in 2 Seconds for changes to take place.");
		thread_sleep(2000);
		reboot_device(FASTBOOT_MODE);
		return;
	}else if (!memcmp(command,"disable_extrom", strlen(command))){
		device_disable_extrom();
		printf("Will Auto-Reboot in 2 Seconds for changes to take place.");
		thread_sleep(2000);
		reboot_device(FASTBOOT_MODE);
		return;
	}else if (!memcmp(command,"init_flsh", strlen(command))){
		cmd_flashlight();
		return;
	}
	printf("HBOOT BUG: Somehow fell through eval_cmd()\n");
	return;
}

void eval_keydown(void){
	switch (keys[keyp]){
		case KEY_VOLUMEUP:
			menu_item_up();
			break;
		case KEY_VOLUMEDOWN:
			menu_item_down();
			break;
		case KEY_SEND: // Dial
			eval_command(0);
			break;
		case KEY_CLEAR: // Hangup
			break;
		case KEY_BACK: // Back
			eval_command(2);
		break;
	}
}
void eval_keyup(void){
	switch (keys[keyp]){
		case KEY_VOLUMEUP:
		case KEY_VOLUMEDOWN:
		case KEY_SEND:
		case KEY_CLEAR:
		case KEY_BACK:
		default:
		break;
	}
}
/*
int usb_listener(void *arg)
{
	for(;;)
	{
		unsigned draw_fastboot_flag_buffer = charger_usb_is_pc_connected();
		if ( (draw_fastboot_flag_buffer) && (draw_fastboot_flag) )	// USB on, flag on
		{
		}
		if ( (draw_fastboot_flag_buffer) && (!(draw_fastboot_flag)) ) // USB on, flag off
		{
		}
		if ( (!(draw_fastboot_flag_buffer)) && (draw_fastboot_flag) ) // USB off, flag on
		{
		}
		if ( (!(draw_fastboot_flag_buffer)) && (!(draw_fastboot_flag)) ) // USB off, flag off
		{
		}
	}
	thread_exit(0);
	return 0;
}
*/
int key_listener(void *arg)
{
	for (;;) 
	{
		for(uint16_t i=0; i< sizeof(keys)/sizeof(uint16_t); i++)
		if (keys_get_state(keys[i]) != 0){
			thread_set_priority(HIGHEST_PRIORITY);
			keyp = i;
			eval_keydown();
			for (uint16_t wcnt = 0; wcnt < 100; wcnt++)
			{
				if(keys_get_state(keys[keyp]) == 0)
				{
					break;
				} else {
					thread_sleep(4);
				}
			}
			while (keys_get_state(keys[keyp]) !=0)
			{
				eval_keydown();
				thread_sleep(80);
			}
			eval_keyup();
			thread_set_priority(LOW_PRIORITY);
		}
	}
	thread_exit(0);
	return 0;
}
void menu_init(void)
{
	if(fbcon_display()==NULL) display_init();

	main_menu.selectedi	 = 0;
	main_menu.maxarl		= 0;
	strcpy(main_menu.id, "HBOOT");

	add_menu_item(&main_menu, "BOOT NAND"  , "boot_nand");
	add_menu_item(&main_menu, "BOOT SD", "boot_sd");
	add_menu_item(&main_menu, "RECOVERY"          , "boot_recv");
	add_menu_item(&main_menu, "FLASHLIGHT"        , "init_flsh");
	add_menu_item(&main_menu, "SETTINGS"          , "goto_sett");
	add_menu_item(&main_menu, "REBOOT"        	, "acpu_ggwp");
	add_menu_item(&main_menu, "REBOOT HBOOT"  , "acpu_bgwp");
	add_menu_item(&main_menu, "POWERDOWN"         , "acpu_pawn");

	sett_menu.selectedi	 = 0;
	sett_menu.maxarl		= 0;
	strcpy(sett_menu.id, "SETTINGS");

	if (device_info.extrom_enabled) 
		add_menu_item(&sett_menu,"ExtROM ENABLED SELECT TO DISABLE !\n","disable_extrom");
	else
		add_menu_item(&sett_menu,"ExtROM DISABLED SELECT TO ENABLE !\n","enable_extrom");
	
	active_menu = &main_menu;

	thread_resume(thread_create("key_listener", &key_listener, 0, LOW_PRIORITY, 4096));
	// thread_resume(thread_create("usb_listener", &usb_listener, 0, LOW_PRIORITY, 4096));

	draw_menu();
}
