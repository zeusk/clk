/*
 * menu.h
 * This file is part of Little Kernel
 *
 * Copyright (C) 2011 - Shantanu/zeusk (shans95g@gmail.com)
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

#define CMD_SZ 16
#define MAX_MENU 16

struct menu_item 
{
	char mTitle[48];
	char command[CMD_SZ];
	
	int x;
	int y;
};

struct menu 
{
	char id[64];
	
	int maxarl;
	int selectedi;

	char back_cmd[CMD_SZ];

	struct menu_item item[MAX_MENU];
};

struct menu main_menu;
struct menu sett_menu;
struct menu rept_menu;
struct menu cust_menu;
struct menu *active_menu;